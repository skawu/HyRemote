#include "composite_target.hpp"

#include <QCoreApplication>
#include <QImage>
#include <QMetaObject>
#include <QPainter>
#include <QThread>

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "detail/composition_support.hpp"
#include "detail/perf_probe.hpp"
#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/frame.hpp"
#include "hyremote/core/storage.hpp"

namespace HyRemote::Runtime::Automatic {
namespace {

struct SurfacePlacement
{
    SurfaceId id = 0;
    QRect globalGeometry;
};

class CompositeCaptureSource final : public hyremote::CaptureSource
{
public:
    CompositeCaptureSource(CompositeTarget *target,
                           ::HyRemote::detail::BuiltinTargetResolver resolver)
        : m_state(std::make_shared<State>())
    {
        m_state->target = target;
        m_state->resolver = std::move(resolver);
    }

    hyremote::CaptureCapabilities capabilities() const override
    {
        hyremote::CaptureCapabilities result;
        result.asynchronous = true;
        result.regionDamage = false;
        result.cpuReadable = true;
        result.cpuFormats = {hyremote::PixelFormat::Rgba8888};
        return result;
    }

    bool start(hyremote::FrameReadyHandler onFrame,
               hyremote::CaptureEventHandler onEvent) override
    {
        QCoreApplication *application = QCoreApplication::instance();
        if (!application)
            return false;

        std::lock_guard<std::mutex> lock(m_state->mutex);
        if (m_state->active || m_state->target.isNull())
            return false;
        if (m_state->target->thread() != application->thread())
            return false;

        m_state->onFrame = std::move(onFrame);
        m_state->onEvent = std::move(onEvent);
        m_state->active = true;

        const std::weak_ptr<State> weakState = m_state;
        m_state->surfaceUnavailableHandler = m_state->target->addSurfaceUnavailableHandler(
            [weakState](SurfaceId id, CompositeTarget::SurfaceLoss loss) {
                if (const auto state = weakState.lock())
                    markSurfaceUnavailable(state, id, loss);
            });
        return true;
    }

    void stop() noexcept override
    {
        std::vector<std::shared_ptr<ChildRuntime>> children;
        QPointer<CompositeTarget> target;
        quint64 surfaceUnavailableHandler = 0;
        {
            std::unique_lock<std::mutex> lock(m_state->mutex);
            m_state->active = false;
            m_state->onFrame = {};
            m_state->onEvent = {};
            m_state->pending.clear();
            m_state->lastCanvasSize = {};
            target = m_state->target;
            surfaceUnavailableHandler = m_state->surfaceUnavailableHandler;
            m_state->surfaceUnavailableHandler = 0;

            m_state->cv.wait(lock, [state = m_state] {
                return state->processingRequests == 0 && state->parentCallbacksInFlight == 0;
            });

            children.reserve(m_state->children.size());
            for (auto &entry : m_state->children)
                children.push_back(std::move(entry.second));
            m_state->children.clear();
        }

        if (target && surfaceUnavailableHandler != 0)
            target->removeSurfaceUnavailableHandler(surfaceUnavailableHandler);

        for (const auto &child : children) {
            if (child && child->capture)
                child->capture->stop();
        }
    }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            if (!m_state->active)
                return false;
        }

        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher)
            return false;

        const std::shared_ptr<State> state = m_state;
        return QMetaObject::invokeMethod(
            dispatcher,
            [state, request] { processRequestOnGuiThread(state, request); },
            Qt::QueuedConnection);
    }

    // #790: the composite publishes a borrowed view of its per-surface sources,
    // so it has no pixels of its own to drop here. It still forwards the one
    // zero-viewer edge, because a child source is the real owner of a grab
    // QImage or a GPU readback buffer. Children are snapshotted as shared_ptrs
    // and released outside the state lock: a child callback may need that lock
    // and stop() may retire the child concurrently.
    void releaseIdleResources() noexcept override
    {
        // #790 review (P1): this is the memory-reclamation path on the capture
        // scheduler, so a bad_alloc from the snapshot must degrade to "children
        // keep their buffer until the next edge or stop()" instead of
        // std::terminate (the #992 failure class). Child callbacks still run
        // outside the state lock; on failure no child state was mutated, the
        // next zero-viewer edge or stop() retries.
        try {
            std::vector<std::shared_ptr<ChildRuntime>> children;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                if (!m_state->active)
                    return;
                children.reserve(m_state->children.size());
                for (const auto &entry : m_state->children)
                    children.push_back(entry.second);
            }
            for (const auto &child : children) {
                if (child && child->capture)
                    child->capture->releaseIdleResources();
            }
        } catch (...) {
            // Allocation failed: drop this edge, never unwind out of the
            // noexcept virtual.
        }
    }

private:
    struct ChildRuntime
    {
        QPointer<QObject> target;
        std::unique_ptr<hyremote::CaptureSource> capture;
        bool ownerRetired = false; // guarded by State::mutex
    };

    struct PendingRequest
    {
        hyremote::CaptureRequest request;
        QRect canvasBounds;
        std::vector<SurfacePlacement> backToFront;
        std::unordered_set<SurfaceId> expected;
        std::unordered_map<SurfaceId, hyremote::RemoteFrame> frames;
    };

    struct State
    {
        std::mutex mutex;
        std::condition_variable cv;
        QPointer<CompositeTarget> target;
        ::HyRemote::detail::BuiltinTargetResolver resolver;
        bool active = false;
        std::size_t processingRequests = 0;
        std::size_t parentCallbacksInFlight = 0;
        quint64 surfaceUnavailableHandler = 0;
        hyremote::FrameReadyHandler onFrame;
        hyremote::CaptureEventHandler onEvent;
        std::unordered_map<SurfaceId, std::shared_ptr<ChildRuntime>> children;
        std::unordered_map<hyremote::CaptureRequestId, PendingRequest> pending;
        // #790: the previously published canvas SIZE, never its pixels. Keeping
        // the last RemoteFrame pinned one full frame (8.29 MB at 1080p, and in
        // the single-surface fast path the child's own grab QImage) for a value
        // only used to preserve geometry while every surface is hidden.
        QSize lastCanvasSize;
    };

    class ProcessingGuard
    {
    public:
        explicit ProcessingGuard(std::shared_ptr<State> state)
            : m_state(std::move(state))
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            if (m_state->active) {
                ++m_state->processingRequests;
                m_entered = true;
            }
        }

        ~ProcessingGuard()
        {
            if (!m_entered)
                return;
            std::lock_guard<std::mutex> lock(m_state->mutex);
            --m_state->processingRequests;
            m_state->cv.notify_all();
        }

        explicit operator bool() const noexcept { return m_entered; }

    private:
        std::shared_ptr<State> m_state;
        bool m_entered = false;
    };

    static bool isCurrentChildLocked(const State &state, SurfaceId id,
                                     const std::weak_ptr<ChildRuntime> &source)
    {
        const auto live = state.children.find(id);
        return live != state.children.end() && live->second
            && !live->second->ownerRetired && live->second == source.lock();
    }

    static void publishEvent(const std::shared_ptr<State> &state,
                             hyremote::CaptureEventCode code,
                             std::string message,
                             bool recoverable = true,
                             std::optional<SurfaceId> childId = std::nullopt,
                             const std::weak_ptr<ChildRuntime> &source = {})
    {
        hyremote::CaptureEventHandler callback;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active || !state->onEvent
                || (childId && !isCurrentChildLocked(*state, *childId, source)))
                return;
            callback = state->onEvent;
            ++state->parentCallbacksInFlight;
        }

        try {
            hyremote::CaptureEvent event;
            event.code = code;
            event.message = std::move(message);
            // Loss/unavailability of one application-owned surface is not loss of the composite
            // application target, so the default is recoverable. Callers that forward a child's own
            // event pass its recoverability through instead of overriding it.
            event.recoverable = recoverable;
            callback(event);
        } catch (...) {
        }

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            --state->parentCallbacksInFlight;
            state->cv.notify_all();
        }
    }

    static void publishFrame(const std::shared_ptr<State> &state, hyremote::RemoteFrame frame)
    {
        hyremote::FrameReadyHandler callback;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active || !state->onFrame)
                return;
            state->lastCanvasSize = QSize(static_cast<int>(frame.geometry.size.width),
                                          static_cast<int>(frame.geometry.size.height));
            callback = state->onFrame;
            ++state->parentCallbacksInFlight;
        }

        HYREMOTE_P1_TRACE_FRAME(CompositeCompleted, frame, 0);
        try {
            callback(std::move(frame));
        } catch (...) {
        }

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            --state->parentCallbacksInFlight;
            state->cv.notify_all();
        }
    }

    static hyremote::RemoteFrame transparentFrame(const hyremote::CaptureRequest &request,
                                                   const QSize &size)
    {
        const int width = qMax(1, size.width());
        const int height = qMax(1, size.height());
        const std::size_t bytesPerLine = static_cast<std::size_t>(width) * 4U;
        auto writable = HyRemote::detail::createWritableCpuFrame(bytesPerLine,
                                                                static_cast<std::size_t>(height));
        if (writable.data) {
            std::memset(writable.data, 0, bytesPerLine * static_cast<std::size_t>(height));
        }

        const hyremote::TimePoint completion = hyremote::Clock::now();
        hyremote::RemoteFrame frame;
        frame.geometry.size = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
        frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
        frame.geometry.alphaMode = hyremote::AlphaMode::Premultiplied;
        frame.geometry.planeCount = 1;
        // #806: composed output allocates exactly one pixel per logical canvas unit.
        // Source surfaces may be 2x DPR, but drawImage scales them into this grid.
        frame.geometry.devicePixelRatio = 1.0F;
        frame.storage = std::move(writable.storage);
        frame.timing.ptsSource = hyremote::PtsSource::Completion;
        frame.timing.requestTime = request.requestTime;
        frame.timing.completionTime = completion;
        frame.damage = hyremote::Damage::fullFrame();
        frame.requestId = request.id;
        return frame;
    }

    static void composeAndPublish(const std::shared_ptr<State> &state, PendingRequest pending)
    {
        HYREMOTE_P1_SCOPE(GenericCompose);
        if (pending.canvasBounds.isEmpty()) {
            // No visible source must not resurrect pixels from a previously
            // visible/authorized composite. Preserve the last framebuffer
            // geometry only, so a legacy RFB Viewer need not support a 1x1
            // DesktopSize transition when the entire scene is hidden.
            QSize clearSize(1, 1);
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (!state->lastCanvasSize.isEmpty())
                    clearSize = state->lastCanvasSize;
            }
            publishFrame(state, transparentFrame(pending.request, clearSize));
            return;
        }

        if (pending.frames.empty()) {
            // Every child failed/vanished: sending lastFrame with a new
            // request ID, new completion timestamp, and full damage falsely
            // advertises old pixels as fresh. Clear the actual canvas instead.
            // Valid children automatically recover on a later request.
            publishFrame(state, transparentFrame(pending.request, pending.canvasBounds.size()));
            return;
        }

        // #609: exactly one already-premultiplied, full-canvas CPU frame needs
        // neither a second 1080p allocation nor a transparent fill + drawImage.
        // QPainter SourceOver onto transparent pixels preserves premultiplied
        // samples exactly. Share the immutable frame's lifetime anchor instead.
        // All scaling, mixed-DPR, clipping, multi-surface and format cases
        // continue through the existing compositor unchanged.
        if (pending.backToFront.size() == 1 && pending.frames.size() == 1) {
            const SurfacePlacement &surface = pending.backToFront.front();
            const auto it = pending.frames.find(surface.id);
            if (it != pending.frames.end() && surface.globalGeometry == pending.canvasBounds) {
                hyremote::RemoteFrame &child = it->second;
                const auto &geometry = child.geometry;
                const std::size_t width = static_cast<std::size_t>(pending.canvasBounds.width());
                const std::size_t height = static_cast<std::size_t>(pending.canvasBounds.height());
                if (geometry.pixelFormat == hyremote::PixelFormat::Rgba8888
                    && geometry.alphaMode == hyremote::AlphaMode::Premultiplied
                    && geometry.planeCount == 1 && geometry.devicePixelRatio == 1.0F
                    && geometry.transform.rotation == hyremote::Rotation::Rotate0
                    && !geometry.transform.mirrorX && !geometry.transform.mirrorY
                    && geometry.size.width == width && geometry.size.height == height
                    && child.storage && child.storage->kind() == hyremote::StorageKind::Cpu
                    && child.storage->planeCount() == 1) {
                    const auto plane = child.storage->mapRead(0);
                    if (plane && plane->data && plane->stride >= width * 4U
                        && height <= plane->bytes / plane->stride) {
                        // Preserve the ordinary composite's per-request metadata,
                        // including DPR=1 and full damage, without mutating pixels.
                        child.timing.ptsSource = hyremote::PtsSource::Completion;
                        child.timing.requestTime = pending.request.requestTime;
                        child.timing.completionTime = hyremote::Clock::now();
                        child.damage = hyremote::Damage::fullFrame();
                        child.requestId = pending.request.id;
                        publishFrame(state, std::move(child));
                        return;
                    }
                }
            }
        }

        hyremote::RemoteFrame result = transparentFrame(pending.request, pending.canvasBounds.size());
        if (!result.storage)
            return;

        const HyRemote::detail::WritableCpuFrame writable = HyRemote::detail::writableCpuFrameOf(result);
        if (!writable.data)
            return;

        const int width = static_cast<int>(result.geometry.size.width);
        const int height = static_cast<int>(result.geometry.size.height);
        const qsizetype bytesPerLine = static_cast<qsizetype>(writable.bytesPerLine);
        QImage output(reinterpret_cast<uchar *>(writable.data),
                      width,
                      height,
                      bytesPerLine,
                      QImage::Format_RGBA8888_Premultiplied);
        output.fill(Qt::transparent);

        QPainter painter(&output);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

        for (const SurfacePlacement &surface : pending.backToFront) {
            auto frameIt = pending.frames.find(surface.id);
            if (frameIt == pending.frames.end())
                continue;

            const hyremote::RemoteFrame &frame = frameIt->second;
            if (frame.geometry.pixelFormat != hyremote::PixelFormat::Rgba8888
                || !frame.storage || frame.geometry.size.width == 0 || frame.geometry.size.height == 0) {
                continue;
            }
            const auto plane = frame.storage->mapRead(0);
            if (!plane || !plane->data || plane->stride == 0)
                continue;

            const QImage::Format format = frame.geometry.alphaMode == hyremote::AlphaMode::Premultiplied
                                              ? QImage::Format_RGBA8888_Premultiplied
                                              : QImage::Format_RGBA8888;
            const QImage source(reinterpret_cast<const uchar *>(plane->data),
                                static_cast<int>(frame.geometry.size.width),
                                static_cast<int>(frame.geometry.size.height),
                                static_cast<qsizetype>(plane->stride),
                                format);
            if (source.isNull())
                continue;

            const QRect destination(surface.globalGeometry.topLeft() - pending.canvasBounds.topLeft(),
                                    surface.globalGeometry.size());
            painter.drawImage(destination, source);
        }
        painter.end();

        // The result is in logical canvas pixels (DPR=1), not child capture pixels.
        // Relabeling it with a child's DPR would double-scale a remote watermark.
        result.timing.completionTime = hyremote::Clock::now();
        publishFrame(state, std::move(result));
    }

    static void tryFinalize(const std::shared_ptr<State> &state,
                            hyremote::CaptureRequestId requestId)
    {
        std::optional<PendingRequest> completed;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            auto it = state->pending.find(requestId);
            if (it == state->pending.end())
                return;
            if (it->second.frames.size() < it->second.expected.size())
                return;
            completed = std::move(it->second);
            state->pending.erase(it);
        }
        composeAndPublish(state, std::move(*completed));
    }

    static void markSurfaceUnavailable(
        const std::shared_ptr<State> &state, SurfaceId id,
        CompositeTarget::SurfaceLoss loss = CompositeTarget::SurfaceLoss::Hidden,
        const std::weak_ptr<ChildRuntime> *source = nullptr)
    {
        std::vector<hyremote::CaptureRequestId> mayComplete;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active || (source && !isCurrentChildLocked(*state, id, *source)))
                return;
            if (loss == CompositeTarget::SurfaceLoss::IdentityChanged) {
                const auto child = state->children.find(id);
                if (child != state->children.end() && child->second)
                    child->second->ownerRetired = true;
            }
            for (auto &entry : state->pending) {
                if (entry.second.expected.erase(id) > 0) {
                    // A surface removed/hidden while another async child is
                    // pending may already have produced a frame. Dropping its
                    // expected token alone is insufficient: the old frame
                    // would still be composited under the current request ID.
                    entry.second.frames.erase(id);
                    mayComplete.push_back(entry.first);
                }
            }
        }
        for (hyremote::CaptureRequestId requestId : mayComplete)
            tryFinalize(state, requestId);
    }

    static void onChildFrame(const std::shared_ptr<State> &state,
                             SurfaceId id,
                             const std::weak_ptr<ChildRuntime> &source,
                             hyremote::RemoteFrame frame)
    {
        if (!frame.requestId)
            return;
        const hyremote::CaptureRequestId requestId = *frame.requestId;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            // SurfaceId may be reused for a different QObject/adapter.
            // A retiring adapter's callbacks cannot represent that source,
            // even if the old request ID happens to be outstanding.
            if (!state->active || !isCurrentChildLocked(*state, id, source))
                return;
            auto pendingIt = state->pending.find(requestId);
            if (pendingIt == state->pending.end()
                || pendingIt->second.expected.find(id) == pendingIt->second.expected.end()) {
                return;
            }
            pendingIt->second.frames[id] = std::move(frame);
        }
        tryFinalize(state, requestId);
    }

    static void onChildEvent(const std::shared_ptr<State> &state,
                             SurfaceId id,
                             const std::weak_ptr<ChildRuntime> &source,
                             const hyremote::CaptureEvent &event)
    {
        markSurfaceUnavailable(state, id, CompositeTarget::SurfaceLoss::Hidden, &source);
        // Propagate the child's own recoverability. Losing one application-owned surface is recoverable -
        // the composite continues with the remaining surfaces - but a child that reports an unrecoverable
        // failure (a destroyed target, a backend that cannot capture at all) must reach Core as
        // unrecoverable, otherwise the session stays Running and silently serves stale or transparent
        // frames with no diagnostic.
        publishEvent(state,
                     event.code,
                     std::string("composite child surface: ") + event.message,
                     event.recoverable, id, source);
    }

    static std::shared_ptr<ChildRuntime> ensureChild(
        const std::shared_ptr<State> &state,
        SurfaceId id,
        QObject *target)
    {
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            auto it = state->children.find(id);
            if (it != state->children.end() && it->second && !it->second->ownerRetired
                && it->second->target.data() == target)
                return it->second;
        }

        ::HyRemote::detail::TargetComponents components = state->resolver(target, false);
        if (!components.supported || !components.capture) {
            publishEvent(state,
                         hyremote::CaptureEventCode::BackendFailure,
                         "no built-in capture adapter supports a composite child surface",
                         /*recoverable=*/false);
            return {};
        }

        auto runtime = std::make_shared<ChildRuntime>();
        runtime->target = target;
        runtime->capture = std::move(components.capture);
        const std::weak_ptr<ChildRuntime> weakRuntime = runtime;
        if (!runtime->capture->start(
                [state, id, weakRuntime](hyremote::RemoteFrame frame) {
                    onChildFrame(state, id, weakRuntime, std::move(frame));
                },
                [state, id, weakRuntime](const hyremote::CaptureEvent &event) {
                    onChildEvent(state, id, weakRuntime, event);
                })) {
            publishEvent(state,
                         hyremote::CaptureEventCode::TemporarilyUnavailable,
                         "failed to start a built-in composite child capture source");
            return {};
        }

        std::shared_ptr<ChildRuntime> previous;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active) {
                previous = runtime;
            } else {
                auto existing = state->children.find(id);
                if (existing != state->children.end())
                    previous = std::move(existing->second);
                state->children[id] = runtime;
            }
        }
        if (previous && previous != runtime && previous->capture)
            previous->capture->stop();
        if (previous == runtime) {
            runtime->capture->stop();
            return {};
        }
        return runtime;
    }

    static void processRequestOnGuiThread(const std::shared_ptr<State> &state,
                                          const hyremote::CaptureRequest &request)
    {
        ProcessingGuard guard(state);
        if (!guard)
            return;

        CompositeTarget *target = state->target.data();
        if (!target) {
            PendingRequest fallback;
            fallback.request = request;
            composeAndPublish(state, std::move(fallback));
            return;
        }

        const CompositeTargetSnapshot snapshot = target->captureSnapshot();

        std::unordered_set<SurfaceId> liveIds;
        std::vector<std::pair<SurfaceId, std::shared_ptr<ChildRuntime>>> candidates;
        PendingRequest pending;
        pending.request = request;
        pending.canvasBounds = snapshot.canvasBounds;
        pending.backToFront.reserve(static_cast<std::size_t>(snapshot.backToFront.size()));

        for (const CompositeSurfaceSnapshot &surface : snapshot.backToFront) {
            QObject *childTarget = surface.target.data();
            if (!childTarget)
                continue;
            liveIds.insert(surface.id);
            pending.backToFront.push_back(SurfacePlacement{surface.id, surface.globalGeometry});
            if (auto child = ensureChild(state, surface.id, childTarget))
                candidates.emplace_back(surface.id, std::move(child));
        }

        std::vector<std::shared_ptr<ChildRuntime>> staleChildren;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            for (auto it = state->children.begin(); it != state->children.end();) {
                if (liveIds.find(it->first) == liveIds.end()) {
                    staleChildren.push_back(std::move(it->second));
                    it = state->children.erase(it);
                } else {
                    ++it;
                }
            }
            for (const auto &candidate : candidates)
                pending.expected.insert(candidate.first);
            state->pending[request.id] = std::move(pending);
        }

        for (const auto &stale : staleChildren) {
            if (stale && stale->capture)
                stale->capture->stop();
        }

        for (const auto &candidate : candidates) {
            if (!candidate.second || !candidate.second->capture
                || !candidate.second->capture->requestFrame(request)) {
                markSurfaceUnavailable(state, candidate.first);
            }
        }

        tryFinalize(state, request.id);
    }

    std::shared_ptr<State> m_state;
};

}  // namespace

CompositeTarget::CompositeTarget(QObject *parent)
    : QObject(parent)
{
}

quint64 CompositeTarget::addSurfaceUnavailableHandler(SurfaceUnavailableHandler handler)
{
    if (!handler)
        return 0;
    const quint64 token = m_nextSurfaceUnavailableHandler++;
    m_surfaceUnavailableHandlers.insert(token, std::move(handler));
    return token;
}

void CompositeTarget::removeSurfaceUnavailableHandler(quint64 token)
{
    if (token != 0)
        m_surfaceUnavailableHandlers.remove(token);
}

void CompositeTarget::notifySurfaceUnavailable(SurfaceId id, SurfaceLoss loss)
{
    // Invoke a snapshot so a handler can unregister itself as part of capture teardown without
    // invalidating this iteration. Surface/model mutations and capture orchestration are GUI-thread
    // owned by the shared automatic runtime; handler state itself remains guarded by the capture source.
    const auto handlers = m_surfaceUnavailableHandlers.values();
    for (const SurfaceUnavailableHandler &handler : handlers) {
        if (handler)
            handler(id, loss);
    }
}

void CompositeTarget::upsertSurface(SurfaceId id,
                                    QObject *target,
                                    const QRect &globalGeometry,
                                    bool visible)
{
    // A stable SurfaceId can be reassigned when the native/Qt object is
    // replaced. Frames already accepted from the previous QObject may still
    // be waiting for another child; invalidate them before the new identity
    // becomes eligible, even if both windows are visible.
    const bool targetReplaced = m_targets.contains(id)
                                && m_targets.value(id).data() != target;
    if (target)
        m_targets[id] = target;
    else
        m_targets.remove(id);
    m_model.upsert(id, globalGeometry, visible);
    if (targetReplaced || !target)
        notifySurfaceUnavailable(id, SurfaceLoss::IdentityChanged);
    else if (!visible)
        notifySurfaceUnavailable(id, SurfaceLoss::Hidden);
}

void CompositeTarget::removeSurface(SurfaceId id)
{
    m_targets.remove(id);
    m_model.remove(id);
    notifySurfaceUnavailable(id, SurfaceLoss::IdentityChanged);
}

void CompositeTarget::setSurfaceVisible(SurfaceId id, bool visible)
{
    m_model.setVisible(id, visible);
    if (!visible)
        notifySurfaceUnavailable(id, SurfaceLoss::Hidden);
}

void CompositeTarget::setSurfaceGeometry(SurfaceId id, const QRect &globalGeometry)
{
    m_model.setGeometry(id, globalGeometry);
}

void CompositeTarget::raiseSurface(SurfaceId id)
{
    m_model.raise(id);
}

CompositeTargetSnapshot CompositeTarget::captureSnapshot() const
{
    CompositeTargetSnapshot snapshot;
    bool haveBounds = false;

    const QVector<SurfaceRecord> ordered = m_model.visibleBackToFront();
    snapshot.backToFront.reserve(ordered.size());
    for (const SurfaceRecord &surface : ordered) {
        const QPointer<QObject> target = m_targets.value(surface.id);
        if (target.isNull())
            continue;
        snapshot.backToFront.push_back(
            CompositeSurfaceSnapshot{surface.id, target, surface.globalGeometry});
        if (!haveBounds) {
            snapshot.canvasBounds = surface.globalGeometry;
            haveBounds = true;
        } else {
            snapshot.canvasBounds = snapshot.canvasBounds.united(surface.globalGeometry);
        }
    }
    if (!haveBounds)
        snapshot.canvasBounds = QRect{};
    return snapshot;
}

::HyRemote::detail::TargetComponents CompositeTarget::createTargetComponents(
    bool remoteInputEnabled,
    const ::HyRemote::detail::BuiltinTargetResolver &resolveBuiltinTarget)
{
    ::HyRemote::detail::TargetComponents result;
    // `supported` describes whether this target can be captured at all, and it stays true even in a build whose
    // adapter options are off: the composite is captured through whatever resolver the caller supplies, which is
    // exactly how the composite tests exercise it. Whether a *particular* surface can be captured is a runtime
    // property and is reported per surface in ensureChild() - claiming support there would be wrong in both
    // directions, and failing closed here overrode the caller's own resolver.
    result.supported = true;
    result.capture = std::make_unique<CompositeCaptureSource>(this, resolveBuiltinTarget);
    if (remoteInputEnabled) {
        result.error = QStringLiteral(
            "multi-surface remote input is not enabled until the composite input-routing gate is present");
    }
    return result;
}

}  // namespace HyRemote::Runtime::Automatic
