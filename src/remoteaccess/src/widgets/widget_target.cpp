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
#include <array>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "detail/input_mailbox_admission.hpp"
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

std::optional<Qt::KeyboardModifier> modifierForQtKey(int key)
{
    switch (key) {
    case Qt::Key_Shift:
        return Qt::ShiftModifier;
    case Qt::Key_Control:
        return Qt::ControlModifier;
    case Qt::Key_Alt:
        return Qt::AltModifier;
    case Qt::Key_Meta:
        return Qt::MetaModifier;
    default:
        return std::nullopt;
    }
}

std::optional<std::size_t> buttonIndex(Qt::MouseButton button)
{
    switch (button) {
    case Qt::LeftButton:
        return 0;
    case Qt::MiddleButton:
        return 1;
    case Qt::RightButton:
        return 2;
    default:
        return std::nullopt;
    }
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

    ~WidgetInputSink() override { shutdown(); }

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

            if (admissionClass == InputMailboxAdmission::Class::ProtectedRelease) {
                if (!state->admission.canAcceptProtectedRelease())
                    throw std::runtime_error("bounded Qt protected-release mailbox is full");
                state->admission.acceptProtectedRelease(event);
                state->pending.push_back(event);
            } else if (event.kind == hyremote::InputEventKind::PointerMove
                       && !state->pending.empty()
                       && state->pending.back().kind == hyremote::InputEventKind::PointerMove) {
                // Pointer motion is freshness-oriented. Adjacent pending moves share one normal
                // admission slot and collapse to the newest coordinate.
                state->pending.back() = event;
            } else {
                if (!state->admission.canAcceptNormal()) {
                    const auto staleMove = std::find_if(
                        state->pending.begin(), state->pending.end(), [](const hyremote::InputEvent &queued) {
                            return queued.kind == hyremote::InputEventKind::PointerMove;
                        });
                    if (staleMove != state->pending.end()) {
                        state->pending.erase(staleMove);
                        state->admission.removePendingNormal();
                    }
                }
                if (!state->admission.canAcceptNormal())
                    throw std::runtime_error("bounded Qt input mailbox is full");

                state->admission.acceptNormal(event);
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
            // Preserve the bounded pending batch and its lifecycle bookkeeping. A later post may
            // successfully schedule the same batch; terminal shutdown will otherwise discard it and
            // balance only state that already reached Qt.
            state->drainScheduled = false;
            throw std::runtime_error("failed to queue remote input drain to the Qt GUI thread");
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
    struct HeldKey
    {
        int key = 0;
        QPointer<QWidget> receiver;
    };

    struct State
    {
        std::mutex mutex;
        QPointer<QWidget> target;
        bool active = true;
        bool drainScheduled = false;
        bool shutdownRequested = false;
        std::deque<hyremote::InputEvent> pending;
        InputMailboxAdmission admission;

        // GUI-thread-owned delivered-state bookkeeping. shutdown() clears pending transport input
        // first, then balances only state that actually reached Qt.
        Qt::MouseButtons buttons = Qt::NoButton;
        std::array<QPointer<QWidget>, 3> buttonReceivers;
        QPoint lastRootPoint;
        bool pointerPositionKnown = false;
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        std::vector<HeldKey> heldKeys;
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

        // Qt's implicit mouse grab: once a widget accepted a press, it keeps receiving pointer moves - and
        // the release - until that button is released, even while the pointer is outside it. Re-resolving a
        // held move with childAt() broke a drag mid-gesture as soon as the pointer left the widget. This
        // extends the existing per-button receiver bookkeeping rather than adding a second routing model.
        if (event.kind == hyremote::InputEventKind::PointerMove && state->buttons != Qt::NoButton) {
            for (const QPointer<QWidget> &remembered : state->buttonReceivers) {
                if (remembered) {
                    receiver = remembered.data();
                    break;
                }
            }
        }

        const Qt::KeyboardModifiers modifiers = toQtModifiers(event.modifiers);
        state->modifiers = modifiers;
        state->lastRootPoint = rootPoint;
        state->pointerPositionKnown = true;

        const Qt::MouseButton button = toQtButton(event.button);
        const auto index = buttonIndex(button);
        if (event.kind == hyremote::InputEventKind::PointerButton && index && !event.pressed) {
            QWidget *pressedReceiver = state->buttonReceivers[*index].data();
            if (pressedReceiver)
                receiver = pressedReceiver;
        }

        const QPoint childPoint = receiver->mapFrom(root, rootPoint);
        const QPoint globalPoint = root->mapToGlobal(rootPoint);

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

        QEvent::Type type = QEvent::MouseMove;
        if (event.kind == hyremote::InputEventKind::PointerButton) {
            if (button == Qt::NoButton || !index)
                return;
            if (event.pressed) {
                state->buttons |= button;
                state->buttonReceivers[*index] = receiver;
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

        if (event.kind == hyremote::InputEventKind::PointerButton && index && !event.pressed)
            state->buttonReceivers[*index].clear();
    }

    static void deliverKey(const std::shared_ptr<State> &state,
                           QWidget *root,
                           const hyremote::InputEvent &event)
    {
        QWidget *receiver = keyboardReceiver(root);
        if (!receiver)
            return;
        const int key = toQtKey(event.key);
        if (key == 0)
            return;

        state->modifiers = toQtModifiers(event.modifiers);
        auto held = std::find_if(state->heldKeys.begin(), state->heldKeys.end(), [key](const HeldKey &entry) {
            return entry.key == key;
        });
        if (event.pressed) {
            if (held == state->heldKeys.end())
                state->heldKeys.push_back(HeldKey{key, receiver});
        } else if (held != state->heldKeys.end()) {
            if (held->receiver)
                receiver = held->receiver.data();

            const auto modifier = modifierForQtKey(key);
            if (!modifier || !(state->modifiers & *modifier))
                state->heldKeys.erase(held);
        }

        QKeyEvent keyEvent(event.pressed ? QEvent::KeyPress : QEvent::KeyRelease,
                           key,
                           state->modifiers);
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

    static void releaseHeldStateOnGuiThread(const std::shared_ptr<State> &state)
    {
        QWidget *root = state->target.data();
        if (!root) {
            state->buttons = Qt::NoButton;
            state->heldKeys.clear();
            return;
        }

        if (state->pointerPositionKnown) {
            static constexpr std::array<Qt::MouseButton, 3> buttons{
                Qt::LeftButton, Qt::MiddleButton, Qt::RightButton};
            for (std::size_t index = 0; index < buttons.size(); ++index) {
                const Qt::MouseButton button = buttons[index];
                if (!(state->buttons & button))
                    continue;
                QWidget *receiver = state->buttonReceivers[index].data();
                if (!receiver)
                    receiver = root;
                state->buttons &= ~Qt::MouseButtons(button);
                const QPoint childPoint = receiver->mapFrom(root, state->lastRootPoint);
                const QPoint globalPoint = root->mapToGlobal(state->lastRootPoint);
                QMouseEvent release(QEvent::MouseButtonRelease,
                                    QPointF(childPoint),
                                    QPointF(state->lastRootPoint),
                                    QPointF(globalPoint),
                                    button,
                                    state->buttons,
                                    state->modifiers);
                QCoreApplication::sendEvent(receiver, &release);
                state->buttonReceivers[index].clear();
            }
        }
        state->buttons = Qt::NoButton;

        const std::vector<HeldKey> held = state->heldKeys;
        auto sendRelease = [&](const HeldKey &entry, Qt::KeyboardModifiers modifiers) {
            QWidget *receiver = entry.receiver.data();
            if (!receiver)
                receiver = root;
            QKeyEvent release(QEvent::KeyRelease, entry.key, modifiers);
            QCoreApplication::sendEvent(receiver, &release);
        };

        for (const HeldKey &entry : held) {
            if (!modifierForQtKey(entry.key))
                sendRelease(entry, state->modifiers);
        }
        for (const HeldKey &entry : held) {
            const auto modifier = modifierForQtKey(entry.key);
            if (!modifier)
                continue;
            state->modifiers &= ~Qt::KeyboardModifiers(*modifier);
            sendRelease(entry, state->modifiers);
        }
        state->heldKeys.clear();
        state->modifiers = Qt::NoModifier;
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
            deliverKey(state, root, event);
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
                state->admission.resetAll();
                state->drainScheduled = false;
                return;
            }
            batch.swap(state->pending);
            state->admission.pendingBatchTaken();
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