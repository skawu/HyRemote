#include "widgets/widget_target.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QImage>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QWidget>
#include <QWindow>
#include <QtMath>

#include <algorithm>
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
#include "detail/qt_window_system_input.hpp"
#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/input.hpp"
#include "hyremote/core/storage.hpp"
#include "hyremote/core/types.hpp"

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

QWidget *keyboardReceiver(QWidget *root)
{
    if (!root)
        return nullptr;
    QWidget *focus = QApplication::focusWidget();
    if (focus && (focus == root || root->isAncestorOf(focus)))
        return focus;
    return root;
}

QWindow *pointerWindow(QWidget *root, QPointF *rootOffset = nullptr)
{
    if (!root)
        return nullptr;
    QWidget *topLevel = root->window();
    if (!topLevel)
        return nullptr;
    if (rootOffset)
        *rootOffset = QPointF(root->mapTo(topLevel, QPoint(0, 0)));
    return topLevel->windowHandle();
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

            const unsigned long acceptedTimestamp = qtWindowSystemTimestamp();
            if (admissionClass == InputMailboxAdmission::Class::ProtectedRelease) {
                if (!state->admission.canAcceptProtectedRelease())
                    throw std::runtime_error("bounded Qt protected-release mailbox is full");
                state->admission.acceptProtectedRelease(event);
            } else {
                // Pointer history is semantic input to Qt. Do not coalesce or evict it and then try to
                // reconstruct the lost meaning in HyRemote. A full bounded lane applies backpressure.
                if (!state->admission.canAcceptNormal())
                    throw std::runtime_error("bounded Qt input mailbox is full");
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
    struct QueuedInput
    {
        hyremote::InputEvent event;
        unsigned long acceptedTimestamp = 0;
    };

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
        std::deque<QueuedInput> pending;
        InputMailboxAdmission admission;

        QtWindowSystemPointerState pointer;
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        std::vector<HeldKey> heldKeys;
    };

    static void deliverPointer(const std::shared_ptr<State> &state,
                               QWidget *root,
                               const hyremote::InputEvent &event,
                               unsigned long timestamp)
    {
        const std::optional<hyremote::MappedInputPoint> mapped =
            hyremote::mapPointerToTarget(event,
                                        static_cast<float>(root->width()),
                                        static_cast<float>(root->height()));
        if (!mapped)
            return;

        QPointF rootOffset;
        QWindow *window = pointerWindow(root, &rootOffset);
        if (!window)
            return;

        const QPointF local = rootOffset + QPointF(mapped->x, mapped->y);
        const QPointF global = window->mapToGlobal(local);
        deliverQtWindowSystemPointer(window, event, local, global, timestamp, state->pointer);
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
            const auto modifier = modifierForQtKey(key);
            if (!modifier || !(state->modifiers & *modifier))
                state->heldKeys.erase(held);
        }

        QKeyEvent keyEvent(event.pressed ? QEvent::KeyPress : QEvent::KeyRelease, key, state->modifiers);
        QCoreApplication::sendEvent(receiver, &keyEvent);
    }

    static void deliverText(QWidget *root, const hyremote::InputEvent &event)
    {
        QWidget *receiver = keyboardReceiver(root);
        if (!receiver || event.textUtf8.empty())
            return;
        QInputMethodEvent inputMethod;
        inputMethod.setCommitString(QString::fromUtf8(event.textUtf8.data(),
                                                      static_cast<qsizetype>(event.textUtf8.size())));
        QCoreApplication::sendEvent(receiver, &inputMethod);
    }

    static void releaseHeldStateOnGuiThread(const std::shared_ptr<State> &state)
    {
        QWidget *root = state->target.data();
        QWindow *window = pointerWindow(root);
        if (window)
            releaseQtWindowSystemPointer(window, qtWindowSystemTimestamp(), state->pointer);
        else
            state->pointer = {};

        if (!root) {
            state->heldKeys.clear();
            state->modifiers = Qt::NoModifier;
            return;
        }

        const std::vector<HeldKey> held = state->heldKeys;
        const auto sendRelease = [](const HeldKey &entry, Qt::KeyboardModifiers modifiers) {
            if (!entry.receiver)
                return;
            QKeyEvent release(QEvent::KeyRelease, entry.key, modifiers);
            QCoreApplication::sendEvent(entry.receiver.data(), &release);
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

    static void deliverOnGuiThread(const std::shared_ptr<State> &state, const QueuedInput &queued)
    {
        QWidget *root = state->target.data();
        if (!root)
            return;

        switch (queued.event.kind) {
        case hyremote::InputEventKind::PointerMove:
        case hyremote::InputEventKind::PointerButton:
        case hyremote::InputEventKind::PointerScroll:
            deliverPointer(state, root, queued.event, queued.acceptedTimestamp);
            break;
        case hyremote::InputEventKind::Key:
            deliverKey(state, root, queued.event);
            break;
        case hyremote::InputEventKind::Text:
            deliverText(root, queued.event);
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
