// R2 QUANTITATIVE RESEARCH ONLY: exact Qt6.8.3, public Qt Quick/Gui.
//
// This verifies what a visible QQuickWindow can actually produce with
// documented PUBLIC APIs; it never dereferences a QRhi/QRhiSwapChain pointer.
// QSGRendererInterface::getResource() indicates an opaque borrowed RESOURCE,
// NOT that a swapchain supports UsedAsTransferSource nor grants CPU readback.
// QtGuiPrivate QRhi::readBackTexture is intentionally not used here.
//
// Modes: scene-only, window-grab, item-grab, rhi-resource-audit.
// Any "resource present" result is informational and NEVER qualifies an
// asynchronous readback backend. Any frame-pixel claim comes from actual
// QImage output after a submitted Qt Quick Scene Graph frame.
#include <QColor>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGRendererInterface>
#include <QSharedPointer>
#include <QThread>
#include <QTimer>
#include <QtGlobal>
#if defined(HYREMOTE_RHI_PRIVATE_INSPECTION_ONLY)
#  include <rhi/qrhi.h> // exact 6.8.3 private Kit; research ONLY
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <ctime>
#endif

namespace {

QColor sourceColor(int phase)
{
    switch (phase % 3) {
    case 0: return QColor(226, 38, 52);
    case 1: return QColor(39, 207, 64);
    default: return QColor(44, 68, 232);
    }
}

bool matches(const QImage &image, int phase)
{
    if (image.isNull() || image.width() <= 0 || image.height() <= 0)
        return false;
    const auto color = image.pixelColor(image.width() / 2, image.height() / 2);
    const QColor expected = sourceColor(phase);
    return std::abs(color.red() - expected.red()) < 16
           && std::abs(color.green() - expected.green()) < 16
           && std::abs(color.blue() - expected.blue()) < 16;
}

double p95(std::vector<double> values)
{
    if (values.empty())
        return -1.0;
    std::sort(values.begin(), values.end());
    const std::size_t index = std::min(values.size() - 1,
        static_cast<std::size_t>(std::ceil(values.size() * 0.95) - 1));
    return values[index];
}

double mean(const std::vector<double> &values)
{
    if (values.empty())
        return -1.0;
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

double processCpuMs()
{
#ifdef _WIN32
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        return -1;
    ULARGE_INTEGER k{}, u{};
    k.LowPart = kernel.dwLowDateTime;
    k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime;
    u.HighPart = user.dwHighDateTime;
    return static_cast<double>(k.QuadPart + u.QuadPart) / 10000.0;
#else
    return static_cast<double>(std::clock()) * 1000.0 / CLOCKS_PER_SEC;
#endif
}

class SolidSceneItem final : public QQuickItem
{
public:
    explicit SolidSceneItem(std::atomic<int> *synchronizedPhase, QQuickItem *parent)
        : QQuickItem(parent), m_synchronizedPhase(synchronizedPhase)
    {
        setFlag(ItemHasContents, true);
    }

    void setPhase(int phase)
    {
        m_phase = phase;
        update();
    }

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override
    {
        auto *node = static_cast<QSGGeometryNode *>(old);
        if (!node) {
            node = new QSGGeometryNode;
            auto *geometry = new QSGGeometry(
                QSGGeometry::defaultAttributes_Point2D(), 4);
            geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
            node->setGeometry(geometry);
            node->setFlag(QSGNode::OwnsGeometry);
            auto *material = new QSGFlatColorMaterial;
            node->setMaterial(material);
            node->setFlag(QSGNode::OwnsMaterial);
        }
        QSGGeometry::updateRectGeometry(node->geometry(), boundingRect());
        node->geometry()->markVertexDataDirty();
        node->markDirty(QSGNode::DirtyGeometry);
        auto *material = static_cast<QSGFlatColorMaterial *>(node->material());
        material->setColor(sourceColor(m_phase));
        node->markDirty(QSGNode::DirtyMaterial);
        m_synchronizedPhase->store(m_phase, std::memory_order_release);
        return node;
    }

private:
    std::atomic<int> *m_synchronizedPhase;
    int m_phase = -1;
};

struct Stats
{
    std::atomic<int> renderCallbacks{0};
    std::atomic<int> afterFrameEnd{0};
    std::atomic<int> lastSubmittedPhase{-1};
    std::atomic<int> observedApi{-1};
    std::atomic<bool> renderThreadDistinct{false};
    std::atomic<bool> rhiResourceObserved{false};
    std::atomic<bool> swapchainResourceObserved{false};
    std::atomic<int> swapchainReadbackFlag{-1};
    std::atomic<int> swapchainFlags{-1};
    std::atomic<bool> commandListObserved{false};
    // 0=beforeRendering,1=beforeRenderPassRecording,
    // 2=afterRenderPassRecording,3=afterRendering,4=afterFrameEnd.
    std::atomic<int> commandListStageMask{0};
    std::atomic<int> rhiStageMask{0};
    std::atomic<int> swapchainStageMask{0};
    std::atomic<bool> redirectTargetObserved{false};
    std::atomic<bool> abortedBySceneGraphLoss{false};

    int correctPixels = 0;
    int readFailures = 0;
    std::vector<double> captureWallMs;
    // These GUI-clock samples include waiting for the ACTUAL afterFrameEnd
    // acknowledgement, unlike capture_wall_* (which measures only the API).
    std::vector<double> sourceToSubmittedMs;
    std::vector<double> sourceToAcceptedMs;
};

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QString mode = QStringLiteral("rhi-resource-audit");
    int width = 640, height = 360;
    int expectedStages = 15;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg.startsWith(QStringLiteral("--mode=")))
            mode = arg.mid(7);
        else if (arg.startsWith(QStringLiteral("--width=")))
            width = arg.mid(8).toInt();
        else if (arg.startsWith(QStringLiteral("--height=")))
            height = arg.mid(9).toInt();
        else if (arg.startsWith(QStringLiteral("--stages=")))
            expectedStages = arg.mid(9).toInt();
        else {
            std::cerr << "R2_VERDICT=BLOCKED_UNKNOWN_ARGUMENT\n";
            return 10;
        }
    }
    if (mode != QLatin1String("rhi-resource-audit")
        && mode != QLatin1String("scene-only")
        && mode != QLatin1String("window-grab")
        && mode != QLatin1String("item-grab")) {
        std::cerr << "R2_VERDICT=BLOCKED_UNKNOWN_MODE\n";
        return 11;
    }
    if (width < 32 || height < 32 || width > 3840 || height > 2160
        || expectedStages < 3 || expectedStages > 100) {
        std::cerr << "R2_VERDICT=BLOCKED_INVALID_WORKLOAD\n";
        return 12;
    }

#ifdef _WIN32
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#endif
    QQuickWindow window;
    window.setColor(Qt::black);
    window.resize(width, height);
    std::atomic<int> synchronizedPhase{-1};
    SolidSceneItem solid(&synchronizedPhase, window.contentItem());
    solid.setSize(QSizeF(width, height));

    Stats stats;
    bool finished = false;
    bool awaitingCapture = false;
    int completed = 0;
    int requested = -1;
    std::chrono::steady_clock::time_point stageStart;
    const auto startingCpu = processCpuMs();
    QElapsedTimer elapsed;
    QSharedPointer<QQuickItemGrabResult> outstanding;

    auto *ri = window.rendererInterface();
    const auto recordStage = [&](int bit) {
        if (!ri) return;
        const int flag = 1 << bit;
        if (ri->getResource(&window, QSGRendererInterface::CommandListResource))
            stats.commandListStageMask.fetch_or(flag, std::memory_order_relaxed);
        if (ri->getResource(&window, QSGRendererInterface::RhiResource))
            stats.rhiStageMask.fetch_or(flag, std::memory_order_relaxed);
        if (ri->getResource(&window, QSGRendererInterface::RhiSwapchainResource))
            stats.swapchainStageMask.fetch_or(flag, std::memory_order_relaxed);
    };
    QObject::connect(&window, &QQuickWindow::beforeRendering,
                     &window, [&] { recordStage(0); }, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::beforeRenderPassRecording,
                     &window, [&] { recordStage(1); }, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::afterRenderPassRecording,
                     &window, [&] { recordStage(2); }, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::afterFrameEnd,
                     &window, [&] { recordStage(4); }, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::afterRendering,
                     &window, [&] {
        recordStage(3);
        stats.renderCallbacks.fetch_add(1, std::memory_order_relaxed);
        // getResource can return NULL outside QSGRenderNode::render(); we
        // record observations ONLY. Null does not establish unavailability.
        if (ri) {
            stats.observedApi.store(static_cast<int>(ri->graphicsApi()),
                                    std::memory_order_release);
            if (ri->getResource(&window, QSGRendererInterface::RhiResource))
                stats.rhiResourceObserved.store(true, std::memory_order_release);
            if (void *sw = ri->getResource(
                    &window, QSGRendererInterface::RhiSwapchainResource)) {
                stats.swapchainResourceObserved.store(true, std::memory_order_release);
#if defined(HYREMOTE_RHI_PRIVATE_INSPECTION_ONLY)
                // This pointer is only borrowed during the render callback.
                // NEVER retain it, mutate flags, or submit a GPU readback.
                // The API contract identifies this as a QRhiSwapChain.
                auto *chain = static_cast<QRhiSwapChain *>(sw);
                const auto flags = chain->flags();
                stats.swapchainFlags.store(flags.toInt(), std::memory_order_release);
                stats.swapchainReadbackFlag.store(
                    flags.testFlag(QRhiSwapChain::UsedAsTransferSource) ? 1 : 0,
                    std::memory_order_release);
#endif
            }
            if (ri->getResource(&window, QSGRendererInterface::CommandListResource))
                stats.commandListObserved.store(true, std::memory_order_release);
            if (ri->getResource(&window, QSGRendererInterface::RhiRedirectRenderTarget))
                stats.redirectTargetObserved.store(true, std::memory_order_release);
        }
        if (QThread::currentThread() != app.thread())
            stats.renderThreadDistinct.store(true, std::memory_order_release);
    }, Qt::DirectConnection);

    QObject::connect(&window, &QQuickWindow::afterFrameEnd,
                     &window, [&] {
        stats.afterFrameEnd.fetch_add(1, std::memory_order_acq_rel);
        const int phase = synchronizedPhase.load(std::memory_order_acquire);
        if (phase >= 0)
            stats.lastSubmittedPhase.store(phase, std::memory_order_release);
    }, Qt::DirectConnection);

    QObject::connect(&window, &QQuickWindow::sceneGraphInvalidated,
                     &window, [&] {
        if (!finished)
            stats.abortedBySceneGraphLoss.store(true, std::memory_order_release);
    }, Qt::DirectConnection);

    auto finish = [&](bool success, const char *verdict) {
        if (finished)
            return;
        finished = true;
        const double wallMs = elapsed.elapsed();
        const double cpuMs = processCpuMs() - startingCpu;
        const int api = stats.observedApi.load(std::memory_order_acquire);
        std::cout << "R2_RESULT_JSON={"
                  << "\"schema\":\"hyremote-qt68-public-rhi-eligibility/1\","
                  << "\"qt\":\"" << QT_VERSION_STR << "\","
                  << "\"mode\":\"" << mode.toStdString() << "\","
                  << "\"graphics_api_enum\":" << api << ","
                  << "\"requested_backend\":\""
#ifdef _WIN32
                  << "d3d11"
#else
                  << "platform-default"
#endif
                  << "\","
                  << "\"size_px\":[" << width << "," << height << "],"
                  << "\"stages_expected\":" << expectedStages << ","
                  << "\"stages_completed\":" << completed << ","
                  << "\"stages_verified_pixels\":" << stats.correctPixels << ","
                  << "\"pixel_truth_verified\":"
                  << (mode == QLatin1String("window-grab") || mode == QLatin1String("item-grab")
                      ? "true" : "false") << ","
                  << "\"render_callbacks\":" << stats.renderCallbacks.load() << ","
                  << "\"after_frame_end\":" << stats.afterFrameEnd.load() << ","
                  << "\"render_thread_distinct_from_gui\":"
                  << (stats.renderThreadDistinct.load() ? "true" : "false") << ","
                  << "\"rhi_resource_observed\":"
                  << (stats.rhiResourceObserved.load() ? "true" : "false") << ","
                  << "\"swapchain_resource_observed\":"
                  << (stats.swapchainResourceObserved.load() ? "true" : "false") << ","
                  << "\"swapchain_transfer_source_flag\":"
                  << stats.swapchainReadbackFlag.load() << ","
                  << "\"swapchain_flags\":" << stats.swapchainFlags.load() << ","
                  << "\"private_inspection_only\":"
#if defined(HYREMOTE_RHI_PRIVATE_INSPECTION_ONLY)
                  << "true,"
#else
                  << "false,"
#endif
                  << "\"command_list_observed\":"
                  << (stats.commandListObserved.load() ? "true" : "false") << ","
                  << "\"command_list_stage_mask\":" << stats.commandListStageMask.load() << ","
                  << "\"rhi_stage_mask\":" << stats.rhiStageMask.load() << ","
                  << "\"swapchain_stage_mask\":" << stats.swapchainStageMask.load() << ","
                  << "\"redirect_target_observed\":"
                  << (stats.redirectTargetObserved.load() ? "true" : "false") << ","
                  << "\"public_rhi_gpu_readback_qualified\":false,"
                  << "\"qtgui_private_used\":"
#if defined(HYREMOTE_RHI_PRIVATE_INSPECTION_ONLY)
                  << "true,"
#else
                  << "false,"
#endif
                  << "\"readback_operations\":0,"
                  << "\"capture_wall_p95_ms\":" << p95(stats.captureWallMs) << ","
                  << "\"capture_wall_mean_ms\":" << mean(stats.captureWallMs) << ","
                  << "\"source_to_submitted_p95_ms\":" << p95(stats.sourceToSubmittedMs) << ","
                  << "\"source_to_submitted_mean_ms\":" << mean(stats.sourceToSubmittedMs) << ","
                  << "\"source_to_accepted_p95_ms\":" << p95(stats.sourceToAcceptedMs) << ","
                  << "\"source_to_accepted_mean_ms\":" << mean(stats.sourceToAcceptedMs) << ","
                  << "\"capture_failures\":" << stats.readFailures << ","
                  << "\"scene_graph_loss\":"
                  << (stats.abortedBySceneGraphLoss.load() ? "true" : "false") << ","
                  << "\"elapsed_ms\":" << wallMs << ","
                  << "\"process_cpu_ms\":" << cpuMs << ","
                  << "\"qualified_for_product\":false"
                  << "}" << std::endl;
        std::cout << "R2_VERDICT=" << (success ? "PASS_PUBLIC_QT68_SCOPED_PROBE" : verdict)
                  << std::endl;
        app.exit(success ? 0 : 20);
    };

    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &app, [&] {
        finish(false, "FAIL_FRAME_NOT_SUBMITTED_OR_CAPTURE_TIMEOUT");
    });

    auto requestNext = [&] {
        if (finished || awaitingCapture)
            return;
        if (completed >= expectedStages) {
            finish(!stats.abortedBySceneGraphLoss.load()
                    && stats.afterFrameEnd.load() >= expectedStages
                    && (mode == QLatin1String("scene-only")
                        || mode == QLatin1String("rhi-resource-audit")
                        || stats.correctPixels == expectedStages)
                    && stats.readFailures == 0,
                   "FAIL_RENDER_OR_PIXEL");
            return;
        }
        requested = completed;
        stageStart = std::chrono::steady_clock::now();
        solid.setPhase(requested);
    };

    QTimer poll;
    poll.setInterval(8);
    QObject::connect(&poll, &QTimer::timeout, &app, [&] {
        if (finished || awaitingCapture || requested < 0)
            return;
        if (stats.lastSubmittedPhase.load(std::memory_order_acquire) < requested)
            return;
        const int expected = requested;
        requested = -1;
        stats.sourceToSubmittedMs.push_back(
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - stageStart).count());
        if (mode == QLatin1String("scene-only")
            || mode == QLatin1String("rhi-resource-audit")) {
            ++completed;
            stats.sourceToAcceptedMs.push_back(
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - stageStart).count());
            QTimer::singleShot(35, &app, requestNext);
            return;
        }

        awaitingCapture = true;
        const auto begin = std::chrono::steady_clock::now();
        if (mode == QLatin1String("window-grab")) {
            const QImage pixels = window.grabWindow(); // DOCUMENTED GUI THREAD
            const double durationMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - begin).count();
            stats.captureWallMs.push_back(durationMs);
            if (!matches(pixels, expected)) {
                ++stats.readFailures;
                finish(false, "FAIL_PUBLIC_WINDOW_GRAB_WRONG_PIXELS");
                return;
            }
            ++stats.correctPixels;
            ++completed;
            stats.sourceToAcceptedMs.push_back(
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - stageStart).count());
            awaitingCapture = false;
            QTimer::singleShot(35, &app, requestNext);
            return;
        }
        outstanding = solid.grabToImage();
        if (!outstanding) {
            ++stats.readFailures;
            finish(false, "FAIL_PUBLIC_ITEM_GRAB_REJECTED");
            return;
        }
        QObject::connect(outstanding.data(), &QQuickItemGrabResult::ready,
                         &app, [&, expected, begin] {
            if (finished)
                return;
            const QImage pixels = outstanding->image();
            const double durationMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - begin).count();
            stats.captureWallMs.push_back(durationMs);
            if (!matches(pixels, expected)) {
                ++stats.readFailures;
                finish(false, "FAIL_PUBLIC_ITEM_GRAB_WRONG_PIXELS");
                return;
            }
            ++stats.correctPixels;
            ++completed;
            stats.sourceToAcceptedMs.push_back(
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - stageStart).count());
            awaitingCapture = false;
            // The finished grab handle may deliver its signal during its own
            // method: release in a later turn, not from within ready().
            auto held = outstanding;
            outstanding.clear();
            QTimer::singleShot(0, &app, [held]() mutable { held.clear(); });
            QTimer::singleShot(35, &app, requestNext);
        });
    });

    window.show();
    elapsed.start();
    watchdog.start(30000);
    poll.start();
    QTimer::singleShot(300, &app, requestNext);
    return app.exec();
}
