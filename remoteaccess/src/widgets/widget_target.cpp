#include "widgets/widget_target.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QImage>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPointer>
#include <QThread>
#include <QWheelEvent>
#include <QWidget>
#include <QtMath>

#include <algorithm>
#include <condition_variable>
#include <cstddef>
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

class WidgetCaptureSource final : public hyremote::CaptureSource
{
public:
    explicit WidgetCaptureSource(QWidget *target)
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
        if (!QCoreApplication::instance())
            return false;

        std::lock_guard<std::mutex> lock(m_state->mutex);
        if (m_state->active || m_state->target.isNull())
            return false;

        // QWidget instances are GUI-thread objects. Capture requests are posted to the
        // QCoreApplication event loop so requestFrame() never touches QWidget from Core's scheduler
        // thread.
        if (m_state->target->thread() != QCoreApplication::instance()->thread())
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

        // A callback already copied out before stop() must finish before this method returns. Queued
        // capture tasks that have not reached callback publication simply observe active=false and
        // become no-ops.
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
            [state, request] { captureOnGuiThread(state, request); },
            Qt::QueuedConnection);
    }

private:
    struct State
    {
        std::mutex mutex;
        std::condition_variable callbacksDrained;
        QPointer<QWidget> target;
        bool active = false;
        std::size_t callbacksInFlight = 0;
        hyremote::FrameReadyHandler onFrame;
        hyremote::CaptureEventHandler onEvent;
    };

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
            // Core callbacks are specified not to leak exceptions through adapter boundaries.
            // Treat an unexpected violation as a dropped callback while preserving stop quiescence.
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

    static void captureOnGuiThread(const std::shared_ptr<State> &state,
                                   const hyremote::CaptureRequest &request)
    {
        QWidget *target = state->target.data();
        if (!target) {
            publishEvent(state,
                         hyremote::CaptureEventCode::TargetLost,
                         "the QWidget target was destroyed",
                         false);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
                return;
        }

        const qreal dpr = target->devicePixelRatioF();
        const int pixelWidth = qMax(1, qCeil(target->width() * dpr));
        const int pixelHeight = qMax(1, qCeil(target->height() * dpr));
        const std::size_t bytesPerLine = static_cast<std::size_t>(pixelWidth) * 4U;

        std::shared_ptr<hyremote::CpuFrameStorage> storage =
            hyremote::CpuFrameStorage::createSinglePlane(bytesPerLine,
                                                         static_cast<std::size_t>(pixelHeight));
        if (!storage || !storage->mutablePlane(0)) {
            publishEvent(state,
                         hyremote::CaptureEventCode::BackendFailure,
                         "failed to allocate owned QWidget frame storage",
                         false);
            return;
        }

        QImage image(reinterpret_cast<uchar *>(storage->mutablePlane(0)),
                     pixelWidth,
                     pixelHeight,
                     static_cast<qsizetype>(bytesPerLine),
                     QImage::Format_RGBA8888_Premultiplied);
        image.setDevicePixelRatio(dpr);
        image.fill(Qt::transparent);
        target->render(&image);

        const hyremote::TimePoint completion = hyremote::Clock::now();
        hyremote::RemoteFrame frame;
        frame.geometry.size = {static_cast<std::uint32_t>(pixelWidth),
                               static_cast<std::uint32_t>(pixelHeight)};
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
    // Qt::KeyboardModifiers has no CapsLock/NumLock state flags. Their transitions are delivered
    // through explicit KeyCode::CapsLock/NumLock key events instead of inventing a Qt modifier.
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

QWidget *keyboardReceiver(QWidget *root)
{
    if (!root)
        return nullptr;
    QWidget *focus = QApplication::focusWidget();
    if (focus && (focus == root || root->isAncestorOf(focus)))
        return focus;
    return root;
}

class WidgetInputSink final : public hyremote::InputSink
{
public:
    explicit WidgetInputSink(QWidget *target)
        : m_state(std::make_shared<State>())
    {
        m_state->target = target;
    }

    ~WidgetInputSink() override
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

            // Pointer motion is freshness-oriented. Adjacent pending moves collapse to the newest
            // coordinate, and when the bounded mailbox is full an older pending move is sacrificed
            // before any key/button/text lifecycle event. Lifecycle-sensitive input is never
            // silently overwritten.
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
                    throw std::runtime_error("bounded Qt input mailbox is full");
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
            throw std::runtime_error("failed to queue remote input drain to the Qt GUI thread");
        }
    }

private:
    static constexpr std::size_t kMaxPendingInputEvents = 64;

    struct State
    {
        std::mutex mutex;
        QPointer<QWidget> target;
        bool active = true;
        bool drainScheduled = false;
        std::deque<hyremote::InputEvent> pending;
        Qt::MouseButtons buttons = Qt::NoButton;  // GUI-thread owned
    };

    static void deliverPointer(const std::shared_ptr<State> &state,
                               QWidget *root,
                               const hyremote::InputEvent &event)
    {
        const std::optional<hyremote::MappedInputPoint> mapped =
            hyremote::mapPointerToTarget(event,
                                        static_cast<float>(root->width()),
                                        static_cast<float>(root->height()));
        if (!mapped)
            return;

        const QPoint rootPoint(qRound(mapped->x), qRound(mapped->y));
        QWidget *receiver = root->childAt(rootPoint);
        if (!receiver)
            receiver = root;

        const QPoint childPoint = receiver->mapFrom(root, rootPoint);
        const QPoint globalPoint = root->mapToGlobal(rootPoint);
        const Qt::KeyboardModifiers modifiers = toQtModifiers(event.modifiers);

        if (event.kind == hyremote::InputEventKind::PointerScroll) {
            QWheelEvent wheel(QPointF(childPoint),
                              QPointF(globalPoint),
                              QPoint(),
                              QPoint(qRound(event.scrollX * 120.0F),
                                     qRound(event.scrollY * 120.0F)),
                              state->buttons,
                              modifiers,
                              Qt::NoScrollPhase,
                              false);
            QCoreApplication::sendEvent(receiver, &wheel);
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
                          QPointF(childPoint),
                          QPointF(rootPoint),
                          QPointF(globalPoint),
                          type == QEvent::MouseMove ? Qt::NoButton : button,
                          state->buttons,
                          modifiers);
        QCoreApplication::sendEvent(receiver, &mouse);
    }

    static void deliverKey(QWidget *root, const hyremote::InputEvent &event)
    {
        QWidget *receiver = keyboardReceiver(root);
        if (!receiver)
            return;
        const int key = toQtKey(event.key);
        if (key == 0)
            return;

        QKeyEvent keyEvent(event.pressed ? QEvent::KeyPress : QEvent::KeyRelease,
                           key,
                           toQtModifiers(event.modifiers));
        QCoreApplication::sendEvent(receiver, &keyEvent);
    }

    static void deliverText(QWidget *root, const hyremote::InputEvent &event)
    {
        if (event.textUtf8.empty())
            return;
        QWidget *receiver = keyboardReceiver(root);
        if (!receiver)
            return;

        QInputMethodEvent inputMethod;
        inputMethod.setCommitString(QString::fromUtf8(event.textUtf8.data(),
                                                      static_cast<qsizetype>(event.textUtf8.size())));
        QCoreApplication::sendEvent(receiver, &inputMethod);
    }

    static void deliverOnGuiThread(const std::shared_ptr<State> &state,
                                   const hyremote::InputEvent &event)
    {
        QWidget *root = state->target.data();
        if (!root)
            return;

        switch (event.kind) {
        case hyremote::InputEventKind::PointerMove:
        case hyremote::InputEventKind::PointerButton:
        case hyremote::InputEventKind::PointerScroll:
            deliverPointer(state, root, event);
            break;
        case hyremote::InputEventKind::Key:
            deliverKey(root, event);
            break;
        case hyremote::InputEventKind::Text:
            deliverText(root, event);
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
            // Clear before delivery so concurrent producers can schedule exactly one next drain.
            // Thus the Qt event queue contains at most one pending drain invocation per sink.
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

TargetComponents createWidgetsTargetComponents(QObject *target, bool remoteInputEnabled)
{
    TargetComponents result;
    auto *widget = qobject_cast<QWidget *>(target);
    if (!widget)
        return result;

    result.supported = true;
    result.capture = std::make_unique<WidgetCaptureSource>(widget);
    if (remoteInputEnabled)
        result.input = std::make_shared<WidgetInputSink>(widget);
    return result;
}

}  // namespace HyRemote::detail
