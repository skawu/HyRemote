#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QTimer>

#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

#include "detail/component_factories.hpp"
#include "hyremote/core/input.hpp"

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

bool pumpUntil(const std::function<bool()> &predicate, int attempts = 100)
{
    for (int i = 0; i < attempts; ++i) {
        if (predicate())
            return true;
        QEventLoop loop;
        QTimer::singleShot(5, &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::AllEvents);
    }
    return predicate();
}

class EventProbe final : public QObject
{
public:
    int moves = 0;
    int buttonPresses = 0;
    int buttonReleases = 0;
    int keyPresses = 0;
    int keyReleases = 0;
    QPointF lastPosition;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched);
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto *mouse = static_cast<QMouseEvent *>(event);
            ++moves;
            lastPosition = mouse->position();
            break;
        }
        case QEvent::MouseButtonPress:
            ++buttonPresses;
            break;
        case QEvent::MouseButtonRelease:
            ++buttonReleases;
            break;
        case QEvent::KeyPress:
            ++keyPresses;
            break;
        case QEvent::KeyRelease:
            ++keyReleases;
            break;
        default:
            break;
        }
        return false;
    }
};

void testPointerFloodCoalescesBeforeGuiDelivery()
{
    HyRemote::detail::resetFactories();

    QQuickWindow window;
    window.resize(100, 50);
    window.show();
    QCoreApplication::processEvents();

    EventProbe probe;
    window.installEventFilter(&probe);

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, true);
    CHECK(components.supported);
    CHECK(components.input != nullptr);

    for (int i = 0; i < 10000; ++i) {
        hyremote::InputEvent move;
        move.kind = hyremote::InputEventKind::PointerMove;
        move.sourceViewport = {100U, 50U, 1.0F};
        move.x = static_cast<float>(i % 100);
        move.y = static_cast<float>(i % 50);
        components.input->post(move);
    }

    CHECK(probe.moves == 0);
    CHECK(pumpUntil([&] { return probe.moves != 0; }));
    CHECK(probe.moves == 1);
    CHECK(std::fabs(probe.lastPosition.x() - 99.0) <= 1.0);
    CHECK(std::fabs(probe.lastPosition.y() - 49.0) <= 1.0);

    components.input.reset();
}

void testProtectedReleaseSurvivesNormalMailboxSaturation()
{
    HyRemote::detail::resetFactories();

    QQuickWindow window;
    window.resize(100, 50);
    window.show();
    window.requestActivate();
    QCoreApplication::processEvents();

    EventProbe probe;
    window.installEventFilter(&probe);

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, true);
    CHECK(components.supported);
    CHECK(components.input != nullptr);

    hyremote::InputEvent button;
    button.kind = hyremote::InputEventKind::PointerButton;
    button.sourceViewport = {100U, 50U, 1.0F};
    button.x = 20.0F;
    button.y = 15.0F;
    button.button = hyremote::PointerButton::Left;
    button.pressed = true;
    components.input->post(button);

    hyremote::InputEvent shift;
    shift.kind = hyremote::InputEventKind::Key;
    shift.key = hyremote::KeyCode::Shift;
    shift.pressed = true;
    shift.modifiers = hyremote::modifierMask(hyremote::InputModifier::Shift);
    components.input->post(shift);

    CHECK(pumpUntil([&] { return probe.buttonPresses == 1 && probe.keyPresses == 1; }));

    for (int i = 0; i < 64; ++i) {
        hyremote::InputEvent text;
        text.kind = hyremote::InputEventKind::Text;
        text.textUtf8 = "x";
        components.input->post(text);
    }

    button.pressed = false;
    shift.pressed = false;
    shift.modifiers = 0U;
    bool protectedReleaseThrew = false;
    try {
        components.input->post(button);
        components.input->post(shift);
    } catch (const std::runtime_error &) {
        protectedReleaseThrew = true;
    }
    CHECK(!protectedReleaseThrew);

    hyremote::InputEvent rejectedPress;
    rejectedPress.kind = hyremote::InputEventKind::Key;
    rejectedPress.key = hyremote::KeyCode::B;
    rejectedPress.pressed = true;
    bool pressRejected = false;
    try {
        components.input->post(rejectedPress);
    } catch (const std::runtime_error &) {
        pressRejected = true;
    }
    CHECK(pressRejected);

    rejectedPress.pressed = false;
    for (int i = 0; i < 256; ++i) {
        bool unmatchedReleaseThrew = false;
        try {
            components.input->post(rejectedPress);
        } catch (const std::runtime_error &) {
            unmatchedReleaseThrew = true;
        }
        CHECK(!unmatchedReleaseThrew);
    }

    CHECK(pumpUntil([&] { return probe.buttonReleases == 1 && probe.keyReleases == 1; }));
    CHECK(probe.buttonPresses == 1);
    CHECK(probe.buttonReleases == 1);
    CHECK(probe.keyPresses == 1);
    CHECK(probe.keyReleases == 1);

    components.input.reset();
}

void testShutdownBalancesDeliveredStateAndDropsPendingInput()
{
    HyRemote::detail::resetFactories();

    QQuickWindow window;
    window.resize(100, 50);
    window.show();
    window.requestActivate();
    QCoreApplication::processEvents();

    EventProbe probe;
    window.installEventFilter(&probe);

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, true);
    CHECK(components.supported);
    CHECK(components.input != nullptr);

    hyremote::InputEvent button;
    button.kind = hyremote::InputEventKind::PointerButton;
    button.sourceViewport = {100U, 50U, 1.0F};
    button.x = 20.0F;
    button.y = 15.0F;
    button.button = hyremote::PointerButton::Left;
    button.pressed = true;
    components.input->post(button);

    hyremote::InputEvent shift;
    shift.kind = hyremote::InputEventKind::Key;
    shift.key = hyremote::KeyCode::Shift;
    shift.pressed = true;
    shift.modifiers = hyremote::modifierMask(hyremote::InputModifier::Shift);
    components.input->post(shift);

    CHECK(pumpUntil([&] { return probe.buttonPresses == 1 && probe.keyPresses == 1; }));
    CHECK(probe.buttonReleases == 0);
    CHECK(probe.keyReleases == 0);

    hyremote::InputEvent pendingKey;
    pendingKey.kind = hyremote::InputEventKind::Key;
    pendingKey.key = hyremote::KeyCode::A;
    pendingKey.pressed = true;
    pendingKey.modifiers = hyremote::modifierMask(hyremote::InputModifier::Shift);
    components.input->post(pendingKey);

    components.input->shutdown();
    CHECK(probe.buttonReleases == 1);
    CHECK(probe.keyReleases == 1);
    CHECK(probe.keyPresses == 1);  // queued A was discarded before QQuickWindow delivery

    QCoreApplication::processEvents();
    CHECK(probe.buttonReleases == 1);
    CHECK(probe.keyReleases == 1);
    CHECK(probe.keyPresses == 1);

    components.input.reset();
    QCoreApplication::processEvents();
    CHECK(probe.buttonReleases == 1);
    CHECK(probe.keyReleases == 1);
}

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    testPointerFloodCoalescesBeforeGuiDelivery();
    testProtectedReleaseSurvivesNormalMailboxSaturation();
    testShutdownBalancesDeliveredStateAndDropsPendingInput();
    HyRemote::detail::resetFactories();
    if (failures != 0)
        std::cerr << failures << " Quick input-backpressure checks failed\n";
    return failures == 0 ? 0 : 1;
}
