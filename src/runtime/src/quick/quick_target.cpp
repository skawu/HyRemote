#include "quick/quick_target.hpp"

#include <QCoreApplication>
#include <QImage>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMetaObject>
#include <QPointer>
#include <QSharedPointer>
#if defined(HYREMOTE_EXPERIMENTAL_PASSIVE_QUICK_GL)
#include <QDebug>
#include <QSGRendererInterface>
#endif
#include <QThread>
#include <QTimer>
#include <QWindow>
#include <QtMath>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickItemGrabResult>
#include <QtQuick/QQuickWindow>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "detail/input_mailbox_admission.hpp"
#include "detail/perf_probe.hpp"
#include "detail/qt_window_system_input.hpp"
#if defined(HYREMOTE_EXPERIMENTAL_PASSIVE_QUICK_GL)
#include "quick/passive_gl_frame_sampler.hpp"
#endif
#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/input.hpp"
#include "hyremote/core/storage.hpp"
#include "hyremote/core/types.hpp"

namespace HyRemote::detail {
namespace {

class QuickCaptureSource final : public hyremote::CaptureSource
{
public:
    explicit QuickCaptureSource(QQuickWindow *target)
        : m_state(std::make_shared<State>())
    {
        m_state->target = target;
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
        QCoreApplication *app = QCoreApplication::instance();
        if (!app)
            return false;

        std::lock_guard<std::mutex> lock(m_state->mutex);
        if (m_state->active || m_state->target.isNull())
            return false;
        if (m_state->target->thread() != app->thread())
            return false;

        m_state->onFrame = std::move(onFrame);
        m_state->onEvent = std::move(onEvent);
        m_state->active = true;
#if defined(HYREMOTE_EXPERIMENTAL_PASSIVE_QUICK_GL)
        // Always install window hooks on the GUI thread. The hook itself runs
        // with DirectConnection on Qt's scenegraph rendering thread.
        const std::weak_ptr<State> weak = m_state;
        if (!QMetaObject::invokeMethod(app, [weak] {
                if (auto state = weak.lock())
                    installPassiveGlHooksOnGuiThread(state);
            }, Qt::QueuedConnection)) {
            m_state->active = false;
            m_state->onFrame = {};
            m_state->onEvent = {};
            return false;
        }
#endif
        return true;
    }

    void stop() noexcept override
    {
        std::unique_lock<std::mutex> lock(m_state->mutex);
        m_state->active = false;
        m_state->onFrame = {};
        m_state->onEvent = {};
#if defined(HYREMOTE_EXPERIMENTAL_PASSIVE_QUICK_GL)
        const QMetaObject::Connection renderConnection
            = std::exchange(m_state->passiveRenderConnection, {});
        const QMetaObject::Connection invalidationConnection
            = std::exchange(m_state->passiveInvalidationConnection, {});
        m_state->passiveEnabled = false;
        m_state->passiveFrame.reset();
        lock.unlock();
        QObject::disconnect(renderConnection);
        QObject::disconnect(invalidationConnection);
        lock.lock();
#endif
        // Ready/timer continuations may still exist after stop, but they all check active before
        // entering a Core callback. A callback that already entered is drained here.
        m_state->callbacksDrained.wait(lock, [this] { return m_state->callbacksInFlight == 0; });
#if defined(HYREMOTE_EXPERIMENTAL_PASSIVE_QUICK_GL)
        qInfo().nospace() << "HYREMOTE_PASSIVE_GL native_captured="
                          << m_state->passiveGlCaptured
                          << " native_rejected=" << m_state->passiveGlRejected
                          << " cache_served=" << m_state->passiveCacheServed
                          << " fallback_requested=" << m_state->passiveFallbackRequested;
#endif
    }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            if (!m_state->active)
                return false;
#if defined(HYREMOTE_EXPERIMENTAL_PASSIVE_QUICK_GL)
            m_state->passiveLastRequest = std::chrono::steady_clock::now();
#endif
        }

        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher)
            return false;

        const std::shared_ptr<State> state = m_state;
        return QMetaObject::invokeMethod(
            dispatcher,
            [state, request] {
#if defined(HYREMOTE_EXPERIMENTAL_PASSIVE_QUICK_GL)
                attemptPassiveOrGrabOnGuiThread(state, request, true);
#else
                attemptGrabOnGuiThread(state, request, 1U);
#endif
            },
            Qt::QueuedConnection);
    }

private:
    static constexpr unsigned int kMaxGrabAttempts = 3U;

    struct State
    {
        std::mutex mutex;
        std::condition_variable callbacksDrained;
        QPointer<QQuickWindow> target;
        bool active = false;
        std::size_t callbacksInFlight = 0;
        hyremote::FrameReadyHandler onFrame;
        hyremote::CaptureEventHandler onEvent;
#if defined(HYREMOTE_EXPERIMENTAL_PASSIVE_QUICK_GL)
        // Render-thread callbacks and Core scheduler requests share only
        // immutable frame storage under mutex; no Core callback on render thread.
        QMetaObject::Connection passiveRenderConnection;
        QMetaObject::Connection passiveInvalidationConnection;
        std::optional<hyremote::RemoteFrame> passiveFrame;
        QSize passiveExpectedSize;
        float passiveExpectedDpr = 1.0F;
        std::chrono::steady_clock::time_point passiveLastRequest{};
        bool passiveEnabled = false;
        bool passiveKickIssued = false;
        std::uint64_t passiveGlCaptured = 0;
        std::uint64_t passiveGlRejected = 0;
        std::uint64_t passiveCacheServed = 0;
        std::uint64_t passiveFallbackRequested = 0;
#endif
    };

    struct GrabAttempt
    {
        bool finished = false;
        QSharedPointer<QQuickItemGrabResult> result;
        QMetaObject::Connection readyConnection;
#if defined(HYREMOTE_ENABLE_CPU_PROBE) && HYREMOTE_ENABLE_CPU_PROBE
        std::chrono::steady_clock::time_point p1GrabStart{};
#endif
    };

#if defined(HYREMOTE_EXPERIMENTAL_PASSIVE_QUICK_GL)
    static void installPassiveGlHooksOnGuiThread(const std::shared_ptr<State> &state)
    {
        QQuickWindow *window = nullptr;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
                return;
            window = state->target.data();
        }
        if (!window || !window->rendererInterface()
            || window->rendererInterface()->graphicsApi() != QSGRendererInterface::OpenGL)
            return; // Non-OpenGL, Qt Quick offscreen and default Windows D3D use the old path.

        const std::weak_ptr<State> weak = state;
        const auto rendered = QObject::connect(
            window, &QQuickWindow::afterRendering, window, [weak, window] {
                const auto current = weak.lock();
                if (!current)
                    return;

                QSize expected;
                float dpr = 1.0F;
                {
                    std::lock_guard<std::mutex> lock(current->mutex);
                    // No capture while viewer demand has stopped. A 350ms lease
                    // covers the 100ms static probe, without a long-lived render
                    // readback after a viewer disconnects.
                    if (!current->active || !current->passiveEnabled
                        || current->passiveExpectedSize.isEmpty()
                        || std::chrono::steady_clock::now() - current->passiveLastRequest
                               > std::chrono::milliseconds(350))
                        return;
                    expected = current->passiveExpectedSize;
                    dpr = current->passiveExpectedDpr;
                }

                auto frame = readPassiveQuickGlFrame(window, expected, dpr);
                std::lock_guard<std::mutex> lock(current->mutex);
                if (!frame) {
                    ++current->passiveGlRejected;
                    return; // Invalid framebuffer: never publish unsupported pixels.
                }
                if (current->active && current->passiveEnabled
                    && current->passiveExpectedSize == expected
                    && current->passiveExpectedDpr == dpr) {
                    current->passiveFrame = std::move(frame);
                    ++current->passiveGlCaptured;
                }
            }, Qt::DirectConnection);
        const auto invalidated = QObject::connect(
            window, &QQuickWindow::sceneGraphInvalidated, window, [weak] {
                if (auto current = weak.lock()) {
                    std::lock_guard<std::mutex> lock(current->mutex);
                    current->passiveFrame.reset();
                    current->passiveKickIssued = false;
                }
            }, Qt::DirectConnection);

        bool accepted = false;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->active && state->target == window) {
                state->passiveRenderConnection = rendered;
                state->passiveInvalidationConnection = invalidated;
                state->passiveEnabled = true;
                accepted = true;
            }
        }
        if (!accepted) {
            QObject::disconnect(rendered);
            QObject::disconnect(invalidated);
        } else {
            qInfo("HYREMOTE_PASSIVE_GL native OpenGL frame-source hook installed");
        }
    }

    static void attemptPassiveOrGrabOnGuiThread(const std::shared_ptr<State> &state,
                                               const hyremote::CaptureRequest &request,
                                               bool mayKickOnce)
    {
        if (!isActive(state))
            return;

        QQuickWindow *window = state->target.data(); // GUI thread only
        if (window && window->isVisible() && window->width() > 0 && window->height() > 0) {
            const float dpr = static_cast<float>(window->devicePixelRatio());
            const QSize expected(qRound(window->width() * dpr),
                                 qRound(window->height() * dpr));
            std::optional<hyremote::RemoteFrame> cached;
            bool requestNativeFrame = false;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (state->passiveEnabled && !expected.isEmpty() && dpr > 0.0F) {
                    if (state->passiveExpectedSize != expected
                        || state->passiveExpectedDpr != dpr) {
                        state->passiveFrame.reset();
                        state->passiveExpectedSize = expected;
                        state->passiveExpectedDpr = dpr;
                        state->passiveKickIssued = false;
                    }
                    if (state->passiveFrame
                        && state->passiveFrame->geometry.size.width
                               == static_cast<std::uint32_t>(expected.width())
                        && state->passiveFrame->geometry.size.height
                               == static_cast<std::uint32_t>(expected.height())) {
                        cached = state->passiveFrame;
                        ++state->passiveCacheServed;
                    } else if (mayKickOnce && !state->passiveKickIssued) {
                        state->passiveKickIssued = true;
                        requestNativeFrame = true;
                    }
                }
            }
            if (cached) {
                cached->timing.ptsSource = hyremote::PtsSource::Completion;
                cached->timing.requestTime = request.requestTime;
                cached->timing.completionTime = hyremote::Clock::now();
                cached->requestId = request.id;
                cached->damage = hyremote::Damage::fullFrame();
                publishCallback(state, &State::onFrame, std::move(*cached));
                return;
            }
            if (requestNativeFrame) {
                // One screen-render bootstrap, not a recurring poll. If no
                // usable native frame arrives quickly, keep legacy grabToImage.
                window->update();
                QCoreApplication *dispatcher = QCoreApplication::instance();
                if (dispatcher) {
                    QTimer::singleShot(50, dispatcher, [state, request] {
                        attemptPassiveOrGrabOnGuiThread(state, request, false);
                    });
                    return;
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            ++state->passiveFallbackRequested;
        }
        attemptGrabOnGuiThread(state, request, 1U); // Always correct fallback
    }
#endif

    static bool isActive(const std::shared_ptr<State> &state)
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        return state->active;
    }

    template <typename Handler, typename Payload>
    static void publishCallback(const std::shared_ptr<State> &state,
                                Handler State::*member,
                                Payload payload)
    {
        Handler callback;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
                return;
            callback = state.get()->*member;
            if (!callback)
                return;
            ++state->callbacksInFlight;
        }

        try {
            callback(std::move(payload));
        } catch (...) {
            // Core callbacks are not allowed to leak exceptions through a target adapter.
        }

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            --state->callbacksInFlight;
            if (state->callbacksInFlight == 0)
                state->callbacksDrained.notify_all();
        }
    }

    static void publishEvent(const std::shared_ptr<State> &state,
                             hyremote::CaptureEventCode code,
                             const char *message,
                             bool recoverable)
    {
        hyremote::CaptureEvent event;
        event.code = code;
        event.message = message;
        event.recoverable = recoverable;
        publishCallback(state, &State::onEvent, std::move(event));
    }

    static void completeUnavailable(const std::shared_ptr<State> &state,
                                    const hyremote::CaptureRequest &request)
    {
        publishEvent(state,
                     hyremote::CaptureEventCode::TemporarilyUnavailable,
                     "Qt Quick capture remained unavailable after the bounded retry budget",
                     true);
        if (!isActive(state))
            return;

        hyremote::RemoteFrame completion;
        completion.timing.ptsSource = hyremote::PtsSource::Completion;
        completion.timing.requestTime = request.requestTime;
        completion.timing.completionTime = hyremote::Clock::now();
        completion.requestId = request.id;
        publishCallback(state, &State::onFrame, std::move(completion));
    }

    static void retryLater(const std::shared_ptr<State> &state,
                           const hyremote::CaptureRequest &request,
                           unsigned int attempt,
                           int delayMs)
    {
        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher || !isActive(state))
            return;

        if (attempt >= kMaxGrabAttempts) {
            completeUnavailable(state, request);
            return;
        }

        QTimer::singleShot(delayMs, dispatcher, [state, request, attempt] {
            attemptGrabOnGuiThread(state, request, attempt + 1U);
        });
    }

    static void publishImage(const std::shared_ptr<State> &state,
                             const hyremote::CaptureRequest &request,
                             unsigned int attempt,
                             const QImage &grabbed)
    {
        if (!isActive(state))
            return;
        if (state->target.isNull()) {
            publishEvent(state,
                         hyremote::CaptureEventCode::TargetLost,
                         "the QQuickWindow target was destroyed",
                         false);
            return;
        }
        if (grabbed.isNull()) {
            retryLater(state, request, attempt, 100);
            return;
        }

        QImage image;
        {
            HYREMOTE_P1_SCOPE(QuickConvert);
            image = grabbed.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        }
        if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
            retryLater(state, request, attempt, 100);
            return;
        }

        const std::size_t bytesPerLine = static_cast<std::size_t>(image.width()) * 4U;
        std::shared_ptr<hyremote::CpuFrameStorage> storage;
        {
            HYREMOTE_P1_SCOPE(QuickBuffer);
            storage = hyremote::CpuFrameStorage::createSinglePlane(
                bytesPerLine, static_cast<std::size_t>(image.height()));
        }
        HYREMOTE_P1_BYTES(QuickBuffer, bytesPerLine * static_cast<std::size_t>(image.height()));
        if (!storage || !storage->mutablePlane(0)) {
            publishEvent(state,
                         hyremote::CaptureEventCode::BackendFailure,
                         "failed to allocate owned Qt Quick frame storage",
                         false);
            return;
        }

        std::byte *destination = storage->mutablePlane(0);
        {
            HYREMOTE_P1_SCOPE(QuickCopy);
            for (int y = 0; y < image.height(); ++y) {
                std::memcpy(destination + static_cast<std::size_t>(y) * bytesPerLine,
                            image.constScanLine(y),
                            bytesPerLine);
            }
        }
        HYREMOTE_P1_BYTES(QuickCopy, bytesPerLine * static_cast<std::size_t>(image.height()));

        const hyremote::TimePoint completion = hyremote::Clock::now();
        hyremote::RemoteFrame frame;
        frame.geometry.size = {static_cast<std::uint32_t>(image.width()),
                               static_cast<std::uint32_t>(image.height())};
        frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
        frame.geometry.alphaMode = hyremote::AlphaMode::Premultiplied;
        frame.geometry.planeCount = 1;
        frame.geometry.devicePixelRatio =
            image.devicePixelRatio() > 0.0 ? static_cast<float>(image.devicePixelRatio()) : 1.0F;
        frame.storage = std::move(storage);
        frame.timing.ptsSource = hyremote::PtsSource::Completion;
        frame.timing.requestTime = request.requestTime;
        frame.timing.completionTime = completion;
        frame.damage = hyremote::Damage::fullFrame();
        frame.requestId = request.id;
        publishCallback(state, &State::onFrame, std::move(frame));
    }

    static void attemptGrabOnGuiThread(const std::shared_ptr<State> &state,
                                       const hyremote::CaptureRequest &request,
                                       unsigned int attempt)
    {
        if (!isActive(state))
            return;

        QQuickWindow *window = state->target.data();
        if (!window) {
            publishEvent(state,
                         hyremote::CaptureEventCode::TargetLost,
                         "the QQuickWindow target was destroyed",
                         false);
            return;
        }

        if (!window->isVisible() || window->visibility() == QWindow::Minimized
            || window->width() <= 0 || window->height() <= 0) {
            retryLater(state, request, attempt, 250);
            return;
        }

        QQuickItem *content = window->contentItem();
        if (!content) {
            publishEvent(state,
                         hyremote::CaptureEventCode::BackendFailure,
                         "QQuickWindow has no content item",
                         false);
            return;
        }

        // grabToImage() takes logical item dimensions and applies DPR itself.
        const QSize logicalSize(qMax(1, qCeil(window->width())),
                                qMax(1, qCeil(window->height())));
        const auto grab = std::make_shared<GrabAttempt>();
#if defined(HYREMOTE_ENABLE_CPU_PROBE) && HYREMOTE_ENABLE_CPU_PROBE
        grab->p1GrabStart = std::chrono::steady_clock::now();
#endif
        {
            HYREMOTE_P1_SCOPE(QuickGrabSubmit);
            grab->result = content->grabToImage(logicalSize);
        }
        if (grab->result.isNull()) {
            retryLater(state, request, attempt, 100);
            return;
        }

        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher)
            return;

        grab->readyConnection = QObject::connect(
            grab->result.data(),
            &QQuickItemGrabResult::ready,
            dispatcher,
            [state, request, attempt, grab] {
                if (grab->finished)
                    return;
                grab->finished = true;
                QObject::disconnect(grab->readyConnection);
#if defined(HYREMOTE_ENABLE_CPU_PROBE) && HYREMOTE_ENABLE_CPU_PROBE
                ::HyRemote::detail::p1perf::addAsyncWall(
                    ::HyRemote::detail::p1perf::Stage::QuickGrabReadyWall,
                    std::chrono::steady_clock::now() - grab->p1GrabStart);
#endif
                const QImage image = grab->result ? grab->result->image() : QImage();
                grab->result.clear();
                publishImage(state, request, attempt, image);
            });

        QTimer::singleShot(2000, dispatcher, [state, request, attempt, grab] {
            if (grab->finished)
                return;
            grab->finished = true;
            QObject::disconnect(grab->readyConnection);
            grab->result.clear();
            if (!isActive(state))
                return;
            if (state->target.isNull()) {
                publishEvent(state,
                             hyremote::CaptureEventCode::TargetLost,
                             "the QQuickWindow target was destroyed during capture",
                             false);
                return;
            }
            retryLater(state, request, attempt, 100);
        });
    }

    std::shared_ptr<State> m_state;
};

Qt::KeyboardModifiers toQtModifiers(hyremote::InputModifiers modifiers)
{
    Qt::KeyboardModifiers result = Qt::NoModifier;
    if (hyremote::hasModifier(modifiers, hyremote::InputModifier::Shift))
        result |= Qt::ShiftModifier;
    if (hyremote::hasModifier(modifiers, hyremote::InputModifier::Control))
        result |= Qt::ControlModifier;
    if (hyremote::hasModifier(modifiers, hyremote::InputModifier::Alt))
        result |= Qt::AltModifier;
    if (hyremote::hasModifier(modifiers, hyremote::InputModifier::Meta))
        result |= Qt::MetaModifier;
    return result;
}

int toQtKey(hyremote::KeyCode key)
{
    using hyremote::KeyCode;
    switch (key) {
    case KeyCode::Enter: return Qt::Key_Return;
    case KeyCode::Escape: return Qt::Key_Escape;
    case KeyCode::Tab: return Qt::Key_Tab;
    case KeyCode::Backspace: return Qt::Key_Backspace;
    case KeyCode::DeleteForward: return Qt::Key_Delete;
    case KeyCode::Insert: return Qt::Key_Insert;
    case KeyCode::Home: return Qt::Key_Home;
    case KeyCode::End: return Qt::Key_End;
    case KeyCode::PageUp: return Qt::Key_PageUp;
    case KeyCode::PageDown: return Qt::Key_PageDown;
    case KeyCode::ArrowLeft: return Qt::Key_Left;
    case KeyCode::ArrowUp: return Qt::Key_Up;
    case KeyCode::ArrowRight: return Qt::Key_Right;
    case KeyCode::ArrowDown: return Qt::Key_Down;
    case KeyCode::Space: return Qt::Key_Space;
    case KeyCode::Shift: return Qt::Key_Shift;
    case KeyCode::Control: return Qt::Key_Control;
    case KeyCode::Alt: return Qt::Key_Alt;
    case KeyCode::Meta: return Qt::Key_Meta;
    case KeyCode::CapsLock: return Qt::Key_CapsLock;
    case KeyCode::NumLock: return Qt::Key_NumLock;
    case KeyCode::Unknown: return 0;
    default: break;
    }

    if (key >= KeyCode::Digit0 && key <= KeyCode::Digit9)
        return Qt::Key_0 + (static_cast<int>(key) - static_cast<int>(KeyCode::Digit0));
    if (key >= KeyCode::A && key <= KeyCode::Z)
        return Qt::Key_A + (static_cast<int>(key) - static_cast<int>(KeyCode::A));
    if (key >= KeyCode::F1 && key <= KeyCode::F12)
        return Qt::Key_F1 + (static_cast<int>(key) - static_cast<int>(KeyCode::F1));
    return 0;
}

std::optional<Qt::KeyboardModifier> modifierForQtKey(int key)
{
    switch (key) {
    case Qt::Key_Shift: return Qt::ShiftModifier;
    case Qt::Key_Control: return Qt::ControlModifier;
    case Qt::Key_Alt: return Qt::AltModifier;
    case Qt::Key_Meta: return Qt::MetaModifier;
    default: return std::nullopt;
    }
}

class QuickInputSink final : public hyremote::InputSink
{
public:
    explicit QuickInputSink(QQuickWindow *target)
        : m_state(std::make_shared<State>())
    {
        m_state->target = target;
    }

    ~QuickInputSink() override { shutdown(); }

    void post(const hyremote::InputEvent &event) override
    {
        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher)
            throw std::runtime_error("Qt application event dispatcher is unavailable");

        const std::shared_ptr<State> state = m_state;
        bool scheduleDrain = false;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
                return;

            const InputMailboxAdmission::Class admissionClass = state->admission.classify(event);
            if (admissionClass == InputMailboxAdmission::Class::DropUnmatchedRelease)
                return;

            const unsigned long acceptedTimestamp = qtWindowSystemTimestamp();
            if (admissionClass == InputMailboxAdmission::Class::ProtectedRelease) {
                if (!state->admission.canAcceptProtectedRelease())
                    throw std::runtime_error("bounded Qt Quick protected-release mailbox is full");
                state->admission.acceptProtectedRelease(event);
            } else {
                if (!state->admission.canAcceptNormal())
                    throw std::runtime_error("bounded Qt Quick input mailbox is full");
                state->admission.acceptNormal(event);
            }
            state->pending.push_back(QueuedInput{event, acceptedTimestamp});

            if (!state->drainScheduled) {
                state->drainScheduled = true;
                scheduleDrain = true;
            }
        }

        if (!scheduleDrain)
            return;

        if (!QMetaObject::invokeMethod(dispatcher, [state] { drainOnGuiThread(state); }, Qt::QueuedConnection)) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->drainScheduled = false;
            throw std::runtime_error("failed to queue remote input drain to the Qt Quick GUI thread");
        }
    }

    void shutdown() noexcept override
    {
        QObject *dispatcher = QCoreApplication::instance();
        const std::shared_ptr<State> state = m_state;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->shutdownRequested)
                return;
            state->shutdownRequested = true;
            state->active = false;
            state->pending.clear();
            state->admission.resetAll();
            state->drainScheduled = false;
        }

        if (!dispatcher)
            return;

        const auto release = [state] { releaseHeldStateOnGuiThread(state); };
        if (QThread::currentThread() == dispatcher->thread()) {
            release();
            return;
        }
        (void)QMetaObject::invokeMethod(dispatcher, release, Qt::QueuedConnection);
    }

private:
    struct QueuedInput
    {
        hyremote::InputEvent event;
        unsigned long acceptedTimestamp = 0;
    };

    struct State
    {
        std::mutex mutex;
        QPointer<QQuickWindow> target;
        bool active = true;
        bool drainScheduled = false;
        bool shutdownRequested = false;
        std::deque<QueuedInput> pending;
        InputMailboxAdmission admission;

        QtWindowSystemPointerState pointer;
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        std::vector<int> heldKeys;
    };

    static void deliverPointer(const std::shared_ptr<State> &state,
                               QQuickWindow *window,
                               const hyremote::InputEvent &event,
                               unsigned long timestamp)
    {
        const std::optional<hyremote::MappedInputPoint> mapped =
            hyremote::mapPointerToTarget(event,
                                        static_cast<float>(window->width()),
                                        static_cast<float>(window->height()));
        if (!mapped)
            return;

        const QPointF local(mapped->x, mapped->y);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPointF global = window->mapToGlobal(local);
#else
        const QPointF global = QPointF(window->mapToGlobal(local.toPoint()));
#endif
        deliverQtWindowSystemPointer(window, event, local, global, timestamp, state->pointer);
    }

    static void deliverKey(const std::shared_ptr<State> &state,
                           QQuickWindow *window,
                           const hyremote::InputEvent &event)
    {
        const int key = toQtKey(event.key);
        if (key == 0)
            return;

        state->modifiers = toQtModifiers(event.modifiers);
        auto held = std::find(state->heldKeys.begin(), state->heldKeys.end(), key);
        if (event.pressed) {
            if (held == state->heldKeys.end())
                state->heldKeys.push_back(key);
        } else if (held != state->heldKeys.end()) {
            const auto modifier = modifierForQtKey(key);
            if (!modifier || !(state->modifiers & *modifier))
                state->heldKeys.erase(held);
        }

        if (event.pressed && !window->isActive()) {
            window->requestActivate();
            window->raise();
        }

        QKeyEvent keyEvent(event.pressed ? QEvent::KeyPress : QEvent::KeyRelease, key, state->modifiers);
        QCoreApplication::sendEvent(window, &keyEvent);
    }

    static void deliverText(QQuickWindow *window, const hyremote::InputEvent &event)
    {
        if (event.textUtf8.empty())
            return;
        QInputMethodEvent inputMethod;
        inputMethod.setCommitString(QString::fromUtf8(event.textUtf8.data(),
                                                      static_cast<qsizetype>(event.textUtf8.size())));
        QCoreApplication::sendEvent(window, &inputMethod);
    }

    static void releaseHeldStateOnGuiThread(const std::shared_ptr<State> &state)
    {
        QQuickWindow *window = state->target.data();
        if (window)
            releaseQtWindowSystemPointer(window, qtWindowSystemTimestamp(), state->pointer);
        else
            state->pointer = {};

        if (!window) {
            state->heldKeys.clear();
            state->modifiers = Qt::NoModifier;
            return;
        }

        const std::vector<int> held = state->heldKeys;
        for (int key : held) {
            if (modifierForQtKey(key))
                continue;
            QKeyEvent release(QEvent::KeyRelease, key, state->modifiers);
            QCoreApplication::sendEvent(window, &release);
        }
        for (int key : held) {
            const auto modifier = modifierForQtKey(key);
            if (!modifier)
                continue;
            state->modifiers &= ~Qt::KeyboardModifiers(*modifier);
            QKeyEvent release(QEvent::KeyRelease, key, state->modifiers);
            QCoreApplication::sendEvent(window, &release);
        }
        state->heldKeys.clear();
        state->modifiers = Qt::NoModifier;
    }

    static void deliverOnGuiThread(const std::shared_ptr<State> &state, const QueuedInput &queued)
    {
        QQuickWindow *window = state->target.data();
        if (!window)
            return;

        switch (queued.event.kind) {
        case hyremote::InputEventKind::PointerMove:
        case hyremote::InputEventKind::PointerButton:
        case hyremote::InputEventKind::PointerScroll:
            deliverPointer(state, window, queued.event, queued.acceptedTimestamp);
            break;
        case hyremote::InputEventKind::Key:
            deliverKey(state, window, queued.event);
            break;
        case hyremote::InputEventKind::Text:
            deliverText(window, queued.event);
            break;
        case hyremote::InputEventKind::None:
            break;
        }
    }

    static void drainOnGuiThread(const std::shared_ptr<State> &state)
    {
        std::deque<QueuedInput> batch;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active) {
                state->pending.clear();
                state->admission.resetAll();
                state->drainScheduled = false;
                return;
            }
            batch.swap(state->pending);
            state->admission.pendingBatchTaken();
            state->drainScheduled = false;
        }

        for (const QueuedInput &queued : batch) {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (!state->active)
                    return;
            }
            deliverOnGuiThread(state, queued);
        }
    }

    std::shared_ptr<State> m_state;
};

}  // namespace

TargetComponents createQuickTargetComponents(QObject *target, bool remoteInputEnabled)
{
    TargetComponents result;
    auto *window = qobject_cast<QQuickWindow *>(target);
    if (!window)
        return result;

    result.supported = true;
    result.capture = std::make_unique<QuickCaptureSource>(window);
    if (remoteInputEnabled)
        result.input = std::make_shared<QuickInputSink>(window);
    return result;
}

}  // namespace HyRemote::detail
