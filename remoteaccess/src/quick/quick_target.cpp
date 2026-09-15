#include "quick/quick_target.hpp"

#include <QCoreApplication>
#include <QImage>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPointer>
#include <QSharedPointer>
#include <QTimer>
#include <QWheelEvent>
#include <QtMath>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickItemGrabResult>
#include <QtQuick/QQuickWindow>

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/input.hpp"
#include "hyremote/core/storage.hpp"

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
        return true;
    }

    void stop() noexcept override
    {
        std::unique_lock<std::mutex> lock(m_state->mutex);
        m_state->active = false;
        m_state->onFrame = {};
        m_state->onEvent = {};

        // Ready/timer continuations may still exist after stop, but they all check active before
        // entering a Core callback. A callback that already entered is drained here.
        m_state->callbacksDrained.wait(lock, [this] { return m_state->callbacksInFlight == 0; });
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
            [state, request] { attemptGrabOnGuiThread(state, request); },
            Qt::QueuedConnection);
    }

private:
    struct State
    {
        std::mutex mutex;
        std::condition_variable callbacksDrained;
        QPointer<QQuickWindow> target;
        bool active = false;
        std::size_t callbacksInFlight = 0;
        hyremote::FrameReadyHandler onFrame;
        hyremote::CaptureEventHandler onEvent;
    };

    struct GrabAttempt
    {
        bool finished = false;  // GUI-thread owned
        QSharedPointer<QQuickItemGrabResult> result;
        QMetaObject::Connection readyConnection;
    };

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

    static void retryLater(const std::shared_ptr<State> &state,
                           const hyremote::CaptureRequest &request,
                           int delayMs)
    {
        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher || !isActive(state))
            return;

        QTimer::singleShot(delayMs, dispatcher, [state, request] {
            attemptGrabOnGuiThread(state, request);
        });
    }

    static void publishImage(const std::shared_ptr<State> &state,
                             const hyremote::CaptureRequest &request,
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
            // A public Quick grab can become unavailable while a window is hidden/minimized. Keep
            // ownership of the Core in-flight slot and retry asynchronously rather than blocking a
            // render/transport thread or leaking the slot.
            retryLater(state, request, 100);
            return;
        }

        const QImage image = grabbed.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
            retryLater(state, request, 100);
            return;
        }

        const std::size_t bytesPerLine = static_cast<std::size_t>(image.width()) * 4U;
        std::shared_ptr<hyremote::CpuFrameStorage> storage =
            hyremote::CpuFrameStorage::createSinglePlane(bytesPerLine,
                                                         static_cast<std::size_t>(image.height()));
        if (!storage || !storage->mutablePlane(0)) {
            publishEvent(state,
                         hyremote::CaptureEventCode::BackendFailure,
                         "failed to allocate owned Qt Quick frame storage",
                         false);
            return;
        }

        std::byte *destination = storage->mutablePlane(0);
        for (int y = 0; y < image.height(); ++y) {
            std::memcpy(destination + static_cast<std::size_t>(y) * bytesPerLine,
                        image.constScanLine(y),
                        bytesPerLine);
        }

        const hyremote::TimePoint completion = hyremote::Clock::now();
        hyremote::RemoteFrame frame;
        frame.geometry.size = {static_cast<std::uint32_t>(image.width()),
                               static_cast<std::uint32_t>(image.height())};
        frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
        frame.geometry.alphaMode = hyremote::AlphaMode::Premultiplied;
        frame.geometry.planeCount = 1;
        frame.storage = std::move(storage);
        frame.timing.ptsSource = hyremote::PtsSource::Completion;
        frame.timing.requestTime = request.requestTime;
        frame.timing.completionTime = completion;
        frame.damage = hyremote::Damage::fullFrame();
        frame.requestId = request.id;
        publishCallback(state, &State::onFrame, std::move(frame));
    }

    static void attemptGrabOnGuiThread(const std::shared_ptr<State> &state,
                                       const hyremote::CaptureRequest &request)
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

        // Accepted requests are retained across a temporary hidden/minimized interval. This is the
        // public-API equivalent of suspending capture: Core's bounded maxInFlight limit prevents an
        // unbounded queue, and showing the same window resumes the outstanding requests.
        if (!window->isVisible() || window->visibility() == QWindow::Minimized
            || window->width() <= 0 || window->height() <= 0) {
            retryLater(state, request, 250);
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

        const qreal dpr = window->devicePixelRatio();
        const QSize pixelSize(qMax(1, qCeil(window->width() * dpr)),
                              qMax(1, qCeil(window->height() * dpr)));

        const auto attempt = std::make_shared<GrabAttempt>();
        attempt->result = content->grabToImage(pixelSize);
        if (attempt->result.isNull()) {
            retryLater(state, request, 100);
            return;
        }

        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher)
            return;

        attempt->readyConnection = QObject::connect(
            attempt->result.data(),
            &QQuickItemGrabResult::ready,
            dispatcher,
            [state, request, attempt] {
                if (attempt->finished)
                    return;
                attempt->finished = true;
                QObject::disconnect(attempt->readyConnection);
                const QImage image = attempt->result ? attempt->result->image() : QImage();
                attempt->result.clear();  // break the result/connection/context lifetime cycle
                publishImage(state, request, image);
            });

        // #16 observed that a public async grab may fail to complete when visibility changes at the
        // wrong moment. Bound that condition: release the result connection and retry the *same*
        // Core request instead of permanently consuming its in-flight slot.
        QTimer::singleShot(2000, dispatcher, [state, request, attempt] {
            if (attempt->finished)
                return;
            attempt->finished = true;
            QObject::disconnect(attempt->readyConnection);
            attempt->result.clear();
            if (!isActive(state))
                return;
            if (state->target.isNull()) {
                publishEvent(state,
                             hyremote::CaptureEventCode::TargetLost,
                             "the QQuickWindow target was destroyed during capture",
                             false);
                return;
            }
            retryLater(state, request, 100);
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

Qt::MouseButton toQtButton(hyremote::PointerButton button)
{
    switch (button) {
    case hyremote::PointerButton::Left:
        return Qt::LeftButton;
    case hyremote::PointerButton::Middle:
        return Qt::MiddleButton;
    case hyremote::PointerButton::Right:
        return Qt::RightButton;
    case hyremote::PointerButton::None:
        return Qt::NoButton;
    }
    return Qt::NoButton;
}

int toQtKey(hyremote::KeyCode key)
{
    using hyremote::KeyCode;
    switch (key) {
    case KeyCode::Enter:
        return Qt::Key_Return;
    case KeyCode::Escape:
        return Qt::Key_Escape;
    case KeyCode::Tab:
        return Qt::Key_Tab;
    case KeyCode::Backspace:
        return Qt::Key_Backspace;
    case KeyCode::DeleteForward:
        return Qt::Key_Delete;
    case KeyCode::Insert:
        return Qt::Key_Insert;
    case KeyCode::Home:
        return Qt::Key_Home;
    case KeyCode::End:
        return Qt::Key_End;
    case KeyCode::PageUp:
        return Qt::Key_PageUp;
    case KeyCode::PageDown:
        return Qt::Key_PageDown;
    case KeyCode::ArrowLeft:
        return Qt::Key_Left;
    case KeyCode::ArrowUp:
        return Qt::Key_Up;
    case KeyCode::ArrowRight:
        return Qt::Key_Right;
    case KeyCode::ArrowDown:
        return Qt::Key_Down;
    case KeyCode::Space:
        return Qt::Key_Space;
    case KeyCode::Shift:
        return Qt::Key_Shift;
    case KeyCode::Control:
        return Qt::Key_Control;
    case KeyCode::Alt:
        return Qt::Key_Alt;
    case KeyCode::Meta:
        return Qt::Key_Meta;
    case KeyCode::CapsLock:
        return Qt::Key_CapsLock;
    case KeyCode::NumLock:
        return Qt::Key_NumLock;
    case KeyCode::Unknown:
        return 0;
    default:
        break;
    }

    if (key >= KeyCode::Digit0 && key <= KeyCode::Digit9)
        return Qt::Key_0 + (static_cast<int>(key) - static_cast<int>(KeyCode::Digit0));
    if (key >= KeyCode::A && key <= KeyCode::Z)
        return Qt::Key_A + (static_cast<int>(key) - static_cast<int>(KeyCode::A));
    if (key >= KeyCode::F1 && key <= KeyCode::F12)
        return Qt::Key_F1 + (static_cast<int>(key) - static_cast<int>(KeyCode::F1));
    return 0;
}

class QuickInputSink final : public hyremote::InputSink
{
public:
    explicit QuickInputSink(QQuickWindow *target)
        : m_state(std::make_shared<State>())
    {
        m_state->target = target;
    }

    ~QuickInputSink() override
    {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        m_state->active = false;
        m_state->pending.clear();
        m_state->drainScheduled = false;
    }

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

            if (event.kind == hyremote::InputEventKind::PointerMove
                && !state->pending.empty()
                && state->pending.back().kind == hyremote::InputEventKind::PointerMove) {
                state->pending.back() = event;
            } else {
                if (state->pending.size() >= kMaxPendingInputEvents) {
                    const auto staleMove = std::find_if(
                        state->pending.begin(), state->pending.end(), [](const hyremote::InputEvent &queued) {
                            return queued.kind == hyremote::InputEventKind::PointerMove;
                        });
                    if (staleMove != state->pending.end())
                        state->pending.erase(staleMove);
                }
                if (state->pending.size() >= kMaxPendingInputEvents)
                    throw std::runtime_error("bounded Qt Quick input mailbox is full");
                state->pending.push_back(event);
            }

            if (!state->drainScheduled) {
                state->drainScheduled = true;
                scheduleDrain = true;
            }
        }

        if (!scheduleDrain)
            return;

        if (!QMetaObject::invokeMethod(
                dispatcher,
                [state] { drainOnGuiThread(state); },
                Qt::QueuedConnection)) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->drainScheduled = false;
            state->pending.clear();
            throw std::runtime_error("failed to queue remote input drain to the Qt Quick GUI thread");
        }
    }

private:
    static constexpr std::size_t kMaxPendingInputEvents = 64;

    struct State
    {
        std::mutex mutex;
        QPointer<QQuickWindow> target;
        bool active = true;
        bool drainScheduled = false;
        std::deque<hyremote::InputEvent> pending;
        Qt::MouseButtons buttons = Qt::NoButton;  // GUI-thread owned
    };

    static void deliverPointer(const std::shared_ptr<State> &state,
                               QQuickWindow *window,
                               const hyremote::InputEvent &event)
    {
        const std::optional<hyremote::MappedInputPoint> mapped =
            hyremote::mapPointerToTarget(event,
                                        static_cast<float>(window->width()),
                                        static_cast<float>(window->height()));
        if (!mapped)
            return;

        const QPoint localPoint(qRound(mapped->x), qRound(mapped->y));
        const QPoint globalPoint = window->mapToGlobal(localPoint);
        const Qt::KeyboardModifiers modifiers = toQtModifiers(event.modifiers);

        if (event.kind == hyremote::InputEventKind::PointerScroll) {
            QWheelEvent wheel(QPointF(localPoint),
                              QPointF(globalPoint),
                              QPoint(),
                              QPoint(qRound(event.scrollX * 120.0F),
                                     qRound(event.scrollY * 120.0F)),
                              state->buttons,
                              modifiers,
                              Qt::NoScrollPhase,
                              false);
            QCoreApplication::sendEvent(window, &wheel);
            return;
        }

        const Qt::MouseButton button = toQtButton(event.button);
        QEvent::Type type = QEvent::MouseMove;
        if (event.kind == hyremote::InputEventKind::PointerButton) {
            if (button == Qt::NoButton)
                return;
            if (event.pressed) {
                state->buttons |= button;
                type = QEvent::MouseButtonPress;
            } else {
                state->buttons &= ~Qt::MouseButtons(button);
                type = QEvent::MouseButtonRelease;
            }
        }

        QMouseEvent mouse(type,
                          QPointF(localPoint),
                          QPointF(localPoint),
                          QPointF(globalPoint),
                          type == QEvent::MouseMove ? Qt::NoButton : button,
                          state->buttons,
                          modifiers);
        QCoreApplication::sendEvent(window, &mouse);
    }

    static void deliverKey(QQuickWindow *window, const hyremote::InputEvent &event)
    {
        const int key = toQtKey(event.key);
        if (key == 0)
            return;
        QKeyEvent keyEvent(event.pressed ? QEvent::KeyPress : QEvent::KeyRelease,
                           key,
                           toQtModifiers(event.modifiers));
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

    static void deliverOnGuiThread(const std::shared_ptr<State> &state,
                                   const hyremote::InputEvent &event)
    {
        QQuickWindow *window = state->target.data();
        if (!window)
            return;

        switch (event.kind) {
        case hyremote::InputEventKind::PointerMove:
        case hyremote::InputEventKind::PointerButton:
        case hyremote::InputEventKind::PointerScroll:
            deliverPointer(state, window, event);
            break;
        case hyremote::InputEventKind::Key:
            deliverKey(window, event);
            break;
        case hyremote::InputEventKind::Text:
            deliverText(window, event);
            break;
        case hyremote::InputEventKind::None:
            break;
        }
    }

    static void drainOnGuiThread(const std::shared_ptr<State> &state)
    {
        std::deque<hyremote::InputEvent> batch;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active) {
                state->pending.clear();
                state->drainScheduled = false;
                return;
            }
            batch.swap(state->pending);
            state->drainScheduled = false;
        }

        for (const hyremote::InputEvent &event : batch) {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (!state->active)
                    return;
            }
            deliverOnGuiThread(state, event);
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
