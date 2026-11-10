// R3: OpenGL RHI-only asynchronous pixel-buffer-object/fence readback.
// PUBLIC Qt 6.8 Quick/Gui/OpenGL APIs; no QRhi, QtGuiPrivate, CPU RFB or
// production interfaces are changed by this test.
//
// A genuine QQuickWindow repeatedly paints four different QSG quadrants.
// Each completed on-screen frame is copied to a GL_PIXEL_PACK_BUFFER, with
// glFenceSync. A following NORMAL scene render polls glClientWaitSync with
// timeout=0, maps the completed PBO, copies ALL CPU pixels, verifies four
// quadrants/phase order, and then retires the fence. Render thread never
// waits for GPU completion. If source stops rendering there is no automatic
// completion: production would need a bounded fallback/retirement policy.
//
// This is NOT EGLFS hardware qualification or a general D3D/Vulkan path.
#include "hyremote/core/frame.hpp"
#include "hyremote/core/storage.hpp"
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGRendererInterface>
#include <QTimer>
#include <QThread>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr int kStages = 30;
constexpr int kSlots = 3;
constexpr GLenum kPackBuffer = 0x88EB; // GL_PIXEL_PACK_BUFFER
constexpr GLenum kPackBinding = 0x88ED; // GL_PIXEL_PACK_BUFFER_BINDING
constexpr GLenum kStreamRead = 0x88E1;  // GL_STREAM_READ
constexpr GLenum kReadBit = 0x0001;     // GL_MAP_READ_BIT
constexpr GLenum kGpuFence = 0x9117;    // GL_SYNC_GPU_COMMANDS_COMPLETE
constexpr GLenum kAlready = 0x911A;     // GL_ALREADY_SIGNALED
constexpr GLenum kSatisfied = 0x911C;   // GL_CONDITION_SATISFIED
constexpr GLenum kTimedOut = 0x911B;    // GL_TIMEOUT_EXPIRED

using Clock = std::chrono::steady_clock;

double millis(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double, std::milli>(b-a).count();
}

QColor stageColor(int phase)
{
    switch ((phase % 3 + 3) % 3) {
    case 0: return QColor(224, 35, 45);
    case 1: return QColor(25, 205, 65);
    default: return QColor(35, 65, 230);
    }
}

bool matches(int r, int g, int b, int phase)
{
    const QColor e=stageColor(phase);
    return std::abs(r-e.red()) <= 14
        && std::abs(g-e.green()) <= 14
        && std::abs(b-e.blue()) <= 14;
}

double p95(std::vector<double> values)
{
    if(values.empty()) return -1;
    std::sort(values.begin(),values.end());
    const size_t index = std::min(values.size()-1,
        size_t(std::ceil(values.size()*.95)-1));
    return values[index];
}

double average(const std::vector<double>& values)
{
    if(values.empty()) return -1;
    return std::accumulate(values.begin(),values.end(),0.0)/values.size();
}

class QuadrantItem final : public QQuickItem
{
public:
    QuadrantItem(std::atomic<int>& synchronized, QQuickItem* parent)
       : QQuickItem(parent), m_synchronized(synchronized)
    {
        setFlag(ItemHasContents,true);
    }
    void next(int phase)
    {
        m_phase=phase;
        update();
    }
protected:
    QSGNode* updatePaintNode(QSGNode* old,UpdatePaintNodeData*) override
    {
        QSGNode* root=old;
        if(!root) {
            root=new QSGNode();
            for(int i=0;i<4;i++) {
                auto* n=new QSGGeometryNode;
                auto* g=new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(),4);
                g->setDrawingMode(QSGGeometry::DrawTriangleStrip);
                n->setGeometry(g);
                n->setFlag(QSGNode::OwnsGeometry);
                auto* mat=new QSGFlatColorMaterial;
                n->setMaterial(mat);
                n->setFlag(QSGNode::OwnsMaterial);
                root->appendChildNode(n);
            }
        }
        auto* node=root->firstChild();
        const qreal halfW=width()/2, halfH=height()/2;
        for(int q=0;q<4 && node;q++) {
            auto* g=static_cast<QSGGeometryNode*>(node);
            const QRectF rect(q%2?halfW:0,q/2?halfH:0,halfW,halfH);
            QSGGeometry::updateRectGeometry(g->geometry(),rect);
            g->geometry()->markVertexDataDirty();
            g->markDirty(QSGNode::DirtyGeometry);
            auto* mat=static_cast<QSGFlatColorMaterial*>(g->material());
            mat->setColor(stageColor(m_phase+q));
            g->markDirty(QSGNode::DirtyMaterial);
            node=node->nextSibling();
        }
        m_synchronized.store(m_phase,std::memory_order_release);
        return root;
    }
private:
    std::atomic<int>& m_synchronized;
    int m_phase=-1;
};

class PboFrameTap final
{
public:
    explicit PboFrameTap(int width,int height,int expected)
       : m_width(width),m_height(height),m_expected(expected),
         m_bytes(static_cast<size_t>(width)*height*4)
    {}

    void onFrame(QQuickWindow& window, int phase)
    {
        m_renderCallbacks.fetch_add(1,std::memory_order_relaxed);
        if(QThread::currentThread()!=QCoreApplication::instance()->thread())
            m_threaded.store(true,std::memory_order_release);
        auto* context=QOpenGLContext::currentContext();
        if(!context
           || window.rendererInterface()->graphicsApi()!=QSGRendererInterface::OpenGL) {
            m_missingContext.fetch_add(1);
            return;
        }
        auto* gl=context->extraFunctions();
        if(!gl || context->format().majorVersion()<3) {
            m_missingGlPbo.fetch_add(1);
            return;
        }
        if(!m_initialized) {
            gl->glGenBuffers(kSlots,m_ids.data());
            for(int i=0;i<kSlots;i++)
                if(!m_ids[i]) m_glErrors.fetch_add(1);
            m_initialized=true;
        }

        GLint oldPbo=0;
        GLint oldPack=4;
        gl->glGetIntegerv(kPackBinding,&oldPbo);
        gl->glGetIntegerv(GL_PACK_ALIGNMENT,&oldPack);
        auto restore=[&] {
            gl->glBindBuffer(kPackBuffer,GLuint(oldPbo));
            gl->glPixelStorei(GL_PACK_ALIGNMENT,oldPack);
        };

        // Retire all ready fences in nonblocking, render-thread-only order.
        for(auto& slot:m_slots) {
            if(!slot.busy) continue;
            const GLenum ready=gl->glClientWaitSync(slot.fence,0,0);
            if(ready==kTimedOut) {
                m_fenceNotReady.fetch_add(1);
                continue;
            }
            if(ready!=kAlready && ready!=kSatisfied) {
                m_glErrors.fetch_add(1);
                continue;
            }
            gl->glBindBuffer(kPackBuffer,slot.pbo);
            const auto mapStart=Clock::now();
            const void* mapped=gl->glMapBufferRange(
                kPackBuffer,0,GLsizeiptr(m_bytes),kReadBit);
            if(!mapped) {
                m_glErrors.fetch_add(1);
            } else {
                // Produce ACTUAL HyRemote Core CpuFrameStorage (not a mock
                // or a borrowed GL pointer). Flip OpenGL bottom-up rows once
                // while writing immutable CPU-owned packed RGBA pixels.
                const std::size_t stride=std::size_t(m_width)*4;
                auto owned=hyremote::CpuFrameStorage::createSinglePlane(
                    stride,std::size_t(m_height));
                auto* destination=owned ? owned->mutablePlane(0):nullptr;
                if(!destination) {
                    m_coreErrors.fetch_add(1);
                } else {
                    const auto* source=static_cast<const std::byte*>(mapped);
                    for(int y=0;y<m_height;y++)
                        std::memcpy(destination+std::size_t(y)*stride,
                                    source+std::size_t(m_height-1-y)*stride,
                                    stride);
                }
                if(gl->glUnmapBuffer(kPackBuffer)!=GL_TRUE)
                    m_glErrors.fetch_add(1);
                m_mapAndCopyMs.push_back(millis(mapStart,Clock::now()));
                if(destination) {
                    const auto view=owned->mapRead(0);
                    if(!view || !cornersMatch(*view,slot.phase)) {
                        m_badPixelFrames.fetch_add(1);
                    } else {
                        hyremote::RemoteFrame frame;
                        frame.geometry.size = {
                            static_cast<std::uint32_t>(m_width),
                            static_cast<std::uint32_t>(m_height)};
                        frame.geometry.pixelFormat=hyremote::PixelFormat::Rgba8888;
                        // This fixture uses opaque rectangles. Real QML
                        // transparency is a distinct, UNQUALIFIED test.
                        frame.geometry.alphaMode=hyremote::AlphaMode::Opaque;
                        frame.geometry.planeCount=1;
                        frame.geometry.devicePixelRatio=1.0f;
                        frame.storage=std::move(owned);
                        frame.damage=hyremote::Damage::fullFrame();
                        frame.requestId=static_cast<hyremote::CaptureRequestId>(
                            slot.phase+1);
                        frame.timing.requestTime=slot.requestTime;
                        frame.timing.completionTime=hyremote::Clock::now();
                        frame.timing.ptsSource=hyremote::PtsSource::Completion;
                        std::string reason;
                        const bool coreValid=
                            hyremote::validateFrame(frame).ok
                            && hyremote::normalizeFrameTiming(frame,&reason)
                            && frame.timing.pts==*frame.timing.completionTime;
                        if(!coreValid || !hyremote::requestToCompletionLatency(frame)) {
                            m_coreErrors.fetch_add(1);
                        } else {
                            if(slot.phase==0) m_firstOwnedFrame=frame;
                            ++m_coreFramesValidated;
                            if(slot.phase!=m_expectedVerified)
                                m_outOfOrder.fetch_add(1);
                            else
                                ++m_expectedVerified;
                            m_completed.store(m_expectedVerified,
                                              std::memory_order_release);
                        }
                    }
                }
                m_endToEndMs.push_back(millis(slot.queuedAt,Clock::now()));
            }
            if(slot.fence)
                gl->glDeleteSync(slot.fence);
            slot.fence=nullptr;
            slot.busy=false;
            ++m_retired;
        }

        if(!m_demand.load(std::memory_order_acquire)) {
            ++m_noDemandRenderFrames;
            if(m_completed.load()==m_expected && m_busyCount()==0)
                cleanup(gl);
            restore();
            return;
        }
        if(phase>=0 && phase<m_expected && phase>m_lastQueued) {
            if(phase!=m_lastQueued+1)
                m_droppedStage.fetch_add(1);
            auto it=std::find_if(m_slots.begin(),m_slots.end(),
                                [](const Slot& slot){return !slot.busy;});
            if(it==m_slots.end()) {
                m_fullRing.fetch_add(1);
                restore();
                return;
            }
            const int slotNo=int(std::distance(m_slots.begin(),it));
            gl->glBindBuffer(kPackBuffer,m_ids[slotNo]);
            gl->glBufferData(kPackBuffer,GLsizeiptr(m_bytes),nullptr,kStreamRead);
            gl->glPixelStorei(GL_PACK_ALIGNMENT,1);
            const auto enqStart=Clock::now();
            gl->glReadPixels(0,0,m_width,m_height,
                             GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            const GLsync fence=gl->glFenceSync(kGpuFence,0);
            // Flush is not a wait. Completion is tested at a future genuine
            // frame; glFinish/glClientWaitSync(timeout>0) are forbidden.
            gl->glFlush();
            const auto enqEnd=Clock::now();
            m_enqueueMs.push_back(millis(enqStart,enqEnd));
            if(!fence || gl->glGetError()!=GL_NO_ERROR) {
                m_glErrors.fetch_add(1);
                if(fence) gl->glDeleteSync(fence);
                restore();
                return;
            }
            it->pbo=m_ids[slotNo];
            it->phase=phase;
            it->queuedAt=enqStart;
            it->requestTime=hyremote::Clock::now();
            it->fence=fence;
            it->busy=true;
            m_lastQueued=phase;
            ++m_queued;
        } else if(phase>=m_expected) {
            ++m_flushRenderFrames;
        }
        restore();
    }

    bool firstFrameStorageStillCorrect() const
    {
        if(!m_firstOwnedFrame || !m_firstOwnedFrame->storage
           || !m_firstOwnedFrame->requestId
           || *m_firstOwnedFrame->requestId!=1)
            return false;
        const auto view=m_firstOwnedFrame->storage->mapRead(0);
        return view && cornersMatch(*view,0);
    }

    void disableDemand() {m_demand.store(false,std::memory_order_release);}

    int verified() const {return m_completed.load(std::memory_order_acquire);}
    int queued() const {return m_queued.load(std::memory_order_acquire);}
    int retired() const {return m_retired.load(std::memory_order_acquire);}
    int errors() const {return m_glErrors.load()+m_missingContext.load()
       +m_missingGlPbo.load()+m_badPixelFrames.load()+m_outOfOrder.load()
       +m_fullRing.load()+m_droppedStage.load()+m_coreErrors.load();}
    int noDemandRenders() const {return m_noDemandRenderFrames.load(std::memory_order_acquire);}
    bool cleaned() const {return m_cleaned.load(std::memory_order_acquire);}
    bool isThreaded() const {return m_threaded.load();}
    int renderCalls() const {return m_renderCallbacks.load();}

    void report(int elapsedMs, const QString& mode, int intervalMs)
    {
        // Only report after the final render thread callback has finished;
        // the test disarms the window connection before reading these.
        std::cout<<"R3_PBO_RESULT_JSON={"
                 <<"\"mode\":\""<<mode.toStdString()<<"\","
                 <<"\"source_interval_ms\":"<<intervalMs<<","
                 <<"\"qt\":\""<<QT_VERSION_STR<<"\","
                 <<"\"backend\":\"Qt6_RHI_OpenGL\","
                 <<"\"render_loop\":\""
                 <<qEnvironmentVariable("QSG_RENDER_LOOP").toStdString()<<"\","
                 <<"\"size_px\":["<<m_width<<","<<m_height<<"],"
                 <<"\"source_stages\":"<<m_expected<<","
                 <<"\"readbacks_enqueued\":"<<m_queued<<","
                 <<"\"readbacks_retired\":"<<m_retired<<","
                 <<"\"verified_full_frames\":"<<m_completed.load()<<","
                 <<"\"core_validated_owned_frames\":"<<m_coreFramesValidated.load()<<","
                 <<"\"core_frame_errors\":"<<m_coreErrors.load()<<","
                 <<"\"first_frame_storage_retained_after_pbo_reuse\":"
                 <<(firstFrameStorageStillCorrect()?"true":"false")<<","

                 <<"\"source_phase_order_errors\":"<<m_outOfOrder.load()<<","
                 <<"\"pixel_errors\":"<<m_badPixelFrames.load()<<","
                 <<"\"gl_errors\":"<<m_glErrors.load()<<","
                 <<"\"missing_context\":"<<m_missingContext.load()<<","
                 <<"\"unsupported_pbo_context\":"<<m_missingGlPbo.load()<<","
                 <<"\"ring_overflows\":"<<m_fullRing.load()<<","
                 <<"\"source_stages_dropped\":"<<m_droppedStage.load()<<","
                 <<"\"fence_not_ready_polls\":"<<m_fenceNotReady.load()<<","
                 <<"\"queued_flush_frames\":"<<m_flushRenderFrames<<","
                 <<"\"zero_viewer_skipped_render_frames\":"<<m_noDemandRenderFrames<<","
                 <<"\"resource_cleanup_on_render_thread\":"
                 <<(m_cleaned?"true":"false")<<","
                 <<"\"render_thread_distinct_from_gui\":"
                 <<(m_threaded.load()?"true":"false")<<","
                 <<"\"render_callback_count\":"<<m_renderCallbacks.load()<<","
                 <<"\"enqueue_p95_ms\":"<<p95(m_enqueueMs)<<","
                 <<"\"enqueue_mean_ms\":"<<average(m_enqueueMs)<<","
                 <<"\"map_copy_p95_ms\":"<<p95(m_mapAndCopyMs)<<","
                 <<"\"completion_lag_p95_ms\":"<<p95(m_endToEndMs)<<","
                 <<"\"elapsed_ms\":"<<elapsedMs<<","
                 <<"\"blocking_fence_wait_calls\":0,"
                 <<"\"qtgui_private_used\":false,"
                 <<"\"qualified_for_product\":false"
                 <<"}"<<std::endl;
    }

private:
    bool cornersMatch(const hyremote::PlaneView& view,int phase) const
    {
        if(!view.data || view.stride<std::size_t(m_width)*4
           || view.bytes<view.stride*std::size_t(m_height))
            return false;
        for(int q=0;q<4;q++) {
            const int x=q%2 ? m_width*3/4:m_width/4;
            const int y=q/2 ? m_height*3/4:m_height/4;
            const std::size_t offset=std::size_t(y)*view.stride
                                    +std::size_t(x)*4;
            const int red=std::to_integer<std::uint8_t>(view.data[offset]);
            const int green=std::to_integer<std::uint8_t>(view.data[offset+1]);
            const int blue=std::to_integer<std::uint8_t>(view.data[offset+2]);
            if(!matches(red,green,blue,phase+q))
                return false;
        }
        return true;
    }

    struct Slot {
        GLuint pbo=0;
        GLsync fence=nullptr;
        int phase=-1;
        Clock::time_point queuedAt;
        hyremote::TimePoint requestTime;
        bool busy=false;
    };

    int m_busyCount() const
    {
        return int(std::count_if(m_slots.begin(),m_slots.end(),
                                 [](const Slot& s){return s.busy;}));
    }

    void cleanup(QOpenGLExtraFunctions* gl)
    {
        if(m_cleaned || m_busyCount()>0) return;
        gl->glDeleteBuffers(kSlots,m_ids.data());
        m_ids.fill(0);
        m_cleaned=true;
    }

    const int m_width,m_height,m_expected;
    const size_t m_bytes;
    std::array<GLuint,kSlots> m_ids{};
    std::array<Slot,kSlots> m_slots{};
    bool m_initialized=false;
    std::atomic<bool> m_cleaned{false};
    int m_lastQueued=-1;
    int m_expectedVerified=0;
    std::atomic<int> m_queued{0};
    std::atomic<int> m_retired{0};
    int m_flushRenderFrames=0;
    std::atomic<int> m_noDemandRenderFrames{0};
    std::atomic<bool> m_demand{true};
    std::atomic<int> m_completed{0},m_glErrors{0},m_missingContext{0};
    std::atomic<int> m_missingGlPbo{0},m_badPixelFrames{0};
    std::atomic<int> m_outOfOrder{0},m_fullRing{0},m_droppedStage{0};
    std::atomic<int> m_fenceNotReady{0},m_renderCallbacks{0};
    std::atomic<int> m_coreErrors{0},m_coreFramesValidated{0};
    std::optional<hyremote::RemoteFrame> m_firstOwnedFrame;
    std::atomic<bool> m_threaded{false};
    std::vector<double> m_enqueueMs,m_mapAndCopyMs,m_endToEndMs;
};

} // namespace

int main(int argc,char** argv)
{
    QGuiApplication app(argc,argv);
    int width=640,height=360;
    int intervalMs=100;
    QString mode=QStringLiteral("dynamic");
    for(int i=1;i<argc;i++) {
        const QString arg=QString::fromLocal8Bit(argv[i]);
        if(arg.startsWith("--width="))
            width=arg.mid(8).toInt();
        else if(arg.startsWith("--height="))
            height=arg.mid(9).toInt();
        else if(arg.startsWith("--interval-ms="))
            intervalMs=arg.mid(14).toInt();
        else if(arg.startsWith("--mode="))
            mode=arg.mid(7);
        else {
            std::cerr<<"R3_VERDICT=BLOCKED_ARG"<<std::endl;
            return 10;
        }
    }
    if(width<32 || height<32 || width>3840 || height>2160 ||
       intervalMs<16 || intervalMs>1000 ||
       (mode!=QLatin1String("dynamic") && mode!=QLatin1String("static-cold")) ||
       qEnvironmentVariable("QSG_RHI_BACKEND")!=QStringLiteral("opengl")) {
        std::cerr<<"R3_VERDICT=BLOCKED_UNSUPPORTED_BACKEND_OR_DIMENSIONS"<<std::endl;
        return 11;
    }

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickWindow window;
    window.setColor(Qt::black);
    window.resize(width,height);

    std::atomic<int> synchronized{-1};
    QuadrantItem item(synchronized,window.contentItem());
    item.setSize(QSizeF(width,height));

    const int required=mode==QLatin1String("static-cold")?1:kStages;
    PboFrameTap tap(width,height,required);
    const auto connected=QObject::connect(&window,&QQuickWindow::afterFrameEnd,
        &window,[&] {
            tap.onFrame(window,synchronized.load(std::memory_order_acquire));
        },Qt::DirectConnection);

    bool done=false,zeroViewerIssued=false;
    int generated=0;
    QElapsedTimer clock;
    QTimer stageTicker;
    stageTicker.setInterval(100);
    QObject::connect(&stageTicker,&QTimer::timeout,&app,[&] {
        if(generated<required) {
            item.next(generated++);
        } else {
            stageTicker.stop();
            if(mode==QLatin1String("static-cold")) {
                // A STATIC scene has no next normal paint. A single bounded
                // call to update() provides a render-context retirement
                // opportunity without changing source content or making a
                // new readback request. The callback must not request more
                // updates on its own (self-trigger loop forbidden).
                window.update();
            } else {
                // Normal dynamic source update provides a later render frame
                // for retiring the final PBO. Never queue another readback.
                item.next(required);
                QTimer::singleShot(200,&app,[&] {
                    if(!done && tap.verified()!=required)
                        item.next(required+1);
                });
            }
        }
    });

    QTimer monitor;
    monitor.setInterval(20);
    QObject::connect(&monitor,&QTimer::timeout,&app,[&] {
        if(done) return;
        if(tap.verified()==required && !zeroViewerIssued) {
            zeroViewerIssued=true;
            tap.disableDemand();
            item.next(required+2); // real local paint with zero Viewer demand
        }
        if(zeroViewerIssued && tap.noDemandRenders()>=1 && tap.cleaned()) {
            done=true;
            stageTicker.stop();
            monitor.stop();
            QObject::disconnect(connected);
            tap.report(clock.elapsed(),mode,intervalMs);
            const bool ok=tap.verified()==required && tap.queued()==required
                          && tap.retired()==required && tap.errors()==0
                          && tap.noDemandRenders()>=1 && tap.cleaned();
            std::cout<<"R3_VERDICT="
                     <<(ok?"PASS_QT68_OPENGL_PBO_ASYNC_SCOPED"
                           :"FAIL_PIXEL_FENCE_LIFETIME_OR_DEMAND")
                     <<std::endl;
            app.exit(ok?0:20);
        }
    });

    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout,&QTimer::timeout,&app,[&] {
        if(done) return;
        done=true;
        stageTicker.stop();
        monitor.stop();
        QObject::disconnect(connected);
        tap.report(clock.elapsed(),mode,intervalMs);
        std::cout<<"R3_VERDICT=FAIL_TIMEOUT_OR_FRAME_NOT_RETIRED"<<std::endl;
        app.exit(21);
    });

    window.show();
    clock.start();
    stageTicker.start(intervalMs);
    timeout.start(20000);
    monitor.start();
    return app.exec();
}
