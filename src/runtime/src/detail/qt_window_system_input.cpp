#include "detail/qt_window_system_input.hpp"

#include <QEvent>
#include <QPoint>
#include <QWindow>
#include <QtMath>

#include <private/qhighdpiscaling_p.h>
#include <qpa/qwindowsysteminterface.h>

#include <array>
#include <chrono>

namespace HyRemote::detail {
namespace {

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

QPointF nativeLocal(const QPointF &logical, QWindow *window)
{
    return QHighDpi::toNativeLocalPosition(logical, window);
}

QPointF nativeGlobal(const QPointF &logical, QWindow *window)
{
    return QHighDpi::toNativeGlobalPosition(logical, window);
}

}  // namespace

unsigned long qtWindowSystemTimestamp() noexcept
{
    // QWindowSystemInterface only requires a monotonic timestamp domain for relative input timing.
    // Keep remote input on its own steady-clock domain: consecutive remote facts preserve their
    // real acceptance spacing, while a local physical press can never accidentally combine with a
    // remote press into one double click merely because both target the same window.
    using namespace std::chrono;
    const auto now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    return static_cast<unsigned long>(now);
}

void deliverQtWindowSystemPointer(QWindow *window,
                                  const hyremote::InputEvent &event,
                                  const QPointF &local,
                                  const QPointF &global,
                                  unsigned long timestamp,
                                  QtWindowSystemPointerState &state)
{
    if (!window)
        return;

    const Qt::KeyboardModifiers modifiers = toQtModifiers(event.modifiers);
    const QPointF nativeLocalPoint = nativeLocal(local, window);
    const QPointF nativeGlobalPoint = nativeGlobal(global, window);

    state.lastLocalPoint = local;
    state.positionKnown = true;

    if (event.kind == hyremote::InputEventKind::PointerScroll) {
        QWindowSystemInterface::handleWheelEvent(window,
                                                 timestamp,
                                                 nativeLocalPoint,
                                                 nativeGlobalPoint,
                                                 QPoint(),
                                                 QPoint(qRound(event.scrollX * 120.0F),
                                                        qRound(event.scrollY * 120.0F)),
                                                 modifiers,
                                                 Qt::NoScrollPhase,
                                                 Qt::MouseEventNotSynthesized,
                                                 false);
        return;
    }

    Qt::MouseButton changedButton = Qt::NoButton;
    QEvent::Type type = QEvent::MouseMove;

    if (event.kind == hyremote::InputEventKind::PointerButton) {
        changedButton = toQtButton(event.button);
        if (changedButton == Qt::NoButton)
            return;

        if (event.pressed) {
            state.buttons |= changedButton;
            type = QEvent::MouseButtonPress;
        } else {
            state.buttons &= ~Qt::MouseButtons(changedButton);
            type = QEvent::MouseButtonRelease;
        }
    } else if (event.kind != hyremote::InputEventKind::PointerMove) {
        return;
    }

    QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(
        window,
        timestamp,
        nativeLocalPoint,
        nativeGlobalPoint,
        state.buttons,
        changedButton,
        type,
        modifiers,
        Qt::MouseEventNotSynthesized);
}

void releaseQtWindowSystemPointer(QWindow *window,
                                  unsigned long timestamp,
                                  QtWindowSystemPointerState &state) noexcept
{
    if (!window || !state.positionKnown) {
        state = {};
        return;
    }

    const QPointF global = window->mapToGlobal(state.lastLocalPoint);
    const QPointF nativeLocalPoint = nativeLocal(state.lastLocalPoint, window);
    const QPointF nativeGlobalPoint = nativeGlobal(global, window);
    static constexpr std::array<Qt::MouseButton, 3> buttons{
        Qt::LeftButton, Qt::MiddleButton, Qt::RightButton};

    for (Qt::MouseButton button : buttons) {
        if (!(state.buttons & button))
            continue;

        state.buttons &= ~Qt::MouseButtons(button);
        QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(
            window,
            timestamp,
            nativeLocalPoint,
            nativeGlobalPoint,
            state.buttons,
            button,
            QEvent::MouseButtonRelease,
            Qt::NoModifier,
            Qt::MouseEventNotSynthesized);
    }

    state = {};
}

}  // namespace HyRemote::detail
