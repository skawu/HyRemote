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

#include "detail/qpa_composition_seam.hpp"
#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/frame.hpp"
#include "hyremote/core/storage.hpp"

namespace HyRemote::Qpa {
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
        return true;
    }

    void stop() noexcept override
    {
        std::vector<std::shared_ptr<ChildRuntime>> children;
        {
            std::unique_lock<std::mutex> lock(m_state->mutex);
            m_state->active = false;
            m_state->onFrame = {};
            m_state->onEvent = {};
            m_state->pending.clear();

            m_state->cv.wait(lock, [state = m_state] {
                return state->processingRequests == 0 && state->parentCallbacksInFlight == 0;
            });

            children.reserve(m_state->children.size());
            for (auto &entry : m_state->children)
                children.push_back(std::move(entry.second));
            m_state->children.clear();
        }

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

private:
    struct ChildRuntime
    {
        QPointer<QObject> target;
        std::unique_ptr<hyremote::CaptureSource> capture;
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
        hyremote::FrameReadyHandler onFrame;
        hyremote::CaptureEventHandler onEvent;
        std::unordered_map<SurfaceId, std::shared_ptr<ChildRuntime>> children;
        std::unordered_map<hyremote::CaptureRequestId, PendingRequest> pending;
        std::optional<hyremote::RemoteFrame> lastFrame;
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

    static void publishEvent(const std::shared_ptr<State> &state,
                             hyremote::CaptureEventCode code,
                             std::string message)
    {
        hyremote::CaptureEventHandler callback;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active || !state->onEvent)
                return;
            callback = state->onEvent;
            ++state->parentCallbacksInFlight;
        }

        try {
            hyremote::CaptureEvent event;
            event.code = code;
            event.message = std::move(message);
            // Loss/unavailability of one application-owned surface is not loss of the composite
            // application target. The model can continue with the remaining surfaces.
            event.recoverable = true;
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
            state->lastFrame = frame;
            callback = state->onFrame;
            ++state->parentCallbacksInFlight;
        }

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
        if (pending.canvasBounds.isEmpty()) {
            std::optional<hyremote::RemoteFrame> last;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (state->lastFrame)
                    last = *state->lastFrame;
            }
            if (last) {
                last->timing.ptsSource = hyremote::PtsSource::Completion;
                last->timing.requestTime = pending.request.requestTime;
                last->timing.completionTime = hyremote::Clock::now();
                last->requestId = pending.request.id;
                last->damage = hyremote::Damage::fullFrame();
                publishFrame(state, std::move(*last));
                return;
            }
            publishFrame(state, transparentFrame(pending.request, QSize(1, 1)));
            return;
        }

        if (pending.frames.empty()) {
            std::optional<hyremote::RemoteFrame> last;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (state->lastFrame
                    && state->lastFrame->geometry.size.width
                           == static_cast<std::uint32_t>(pending.canvasBounds.width())
                    && state->lastFrame->geometry.size.height
                           == static_cast<std::uint32_t>(pending.canvasBounds.height())) {
                    last = *state->lastFrame;
                }
            }
            if (last) {
                last->timing.ptsSource = hyremote::PtsSource::Completion;
                last->timing.requestTime = pending.request.requestTime;
                last->timing.completionTime = hyremote::Clock::now();
                last->requestId = pending.request.id;
                last->damage = hyremote::Damage::fullFrame();
                publishFrame(state, std::move(*last));
                return;
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

    static void markSurfaceUnavailable(const std::shared_ptr<State> &state, SurfaceId id)
    {
        std::vector<hyremote::CaptureRequestId> mayComplete;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            for (auto &entry : state->pending) {
                if (entry.second.expected.erase(id) > 0)
                    mayComplete.push_back(entry.first);
            }
        }
        for (hyremote::CaptureRequestId requestId : mayComplete)
            tryFinalize(state, requestId);
    }

    static void onChildFrame(const std::shared_ptr<State> &state,
                             SurfaceId id,
                             hyremote::RemoteFrame frame)
    {
        if (!frame.requestId)
            return;
        const hyremote::CaptureRequestId requestId = *frame.requestId;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
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
                             const hyremote::CaptureEvent &event)
    {
        markSurfaceUnavailable(state, id);
        publishEvent(state,
                     event.code,
                     std::string("composite child surface: ") + event.message);
    }

    static std::shared_ptr<ChildRuntime> ensureChild(
        const std::shared_ptr<State> &state,
        SurfaceId id,
        QObject *target)
    {
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            auto it = state->children.find(id);
            if (it != state->children.end() && it->second && it->second->target.data() == target)
                return it->second;
        }

        ::HyRemote::detail::TargetComponents components = state->resolver(target, false);
        if (!components.supported || !components.capture) {
            publishEvent(state,
                         hyremote::CaptureEventCode::TemporarilyUnavailable,
                         "no built-in capture adapter supports a composite child surface");
            return {};
        }

        auto runtime = std::make_shared<ChildRuntime>();
        runtime->target = target;
        runtime->capture = std::move(components.capture);
        if (!runtime->capture->start(
                [state, id](hyremote::RemoteFrame frame) {
                    onChildFrame(state, id, std::move(frame));
                },
                [state, id](const hyremote::CaptureEvent &event) {
                    onChildEvent(state, id, event);
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

void CompositeTarget::upsertSurface(SurfaceId id,
                                    QObject *target,
                                    const QRect &globalGeometry,
                                    bool visible)
{
    if (target)
        m_targets[id] = target;
    else
        m_targets.remove(id);
    m_model.upsert(id, globalGeometry, visible);
}

void CompositeTarget::removeSurface(SurfaceId id)
{
    m_targets.remove(id);
    m_model.remove(id);
}

void CompositeTarget::setSurfaceVisible(SurfaceId id, bool visible)
{
    m_model.setVisible(id, visible);
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
    result.supported = true;
    result.capture = std::make_unique<CompositeCaptureSource>(this, resolveBuiltinTarget);
    if (remoteInputEnabled) {
        result.error = QStringLiteral(
            "QPA multi-surface remote input is not enabled until the composite input-routing gate is present");
    }
    return result;
}

}  // namespace HyRemote::Qpa
