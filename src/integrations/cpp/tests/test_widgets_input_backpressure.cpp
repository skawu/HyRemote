#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWidget>

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

class ProbeWidget final : public QWidget
{
public:
    int moves = 0;
    int buttonPresses = 0;
    int buttonReleases = 0;
    int keyPresses = 0;
    int keyReleases = 0;
    QPointF lastPosition;

protected:
    void mouseMoveEvent(QMouseEvent *event) override
    {
        ++moves;
        lastPosition = event->position();
        event->accept();
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        ++buttonPresses;
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        ++buttonReleases;
        event->accept();
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        ++keyPresses;
        event->accept();
    }

    void keyReleaseEvent(QKeyEvent *event) override
    {
        ++keyReleases;
        event->accept();
    }
};

void testPointerFloodCoalescesBeforeGuiDelivery()
{
    HyRemote::detail::resetFactories();

    ProbeWidget target;
    target.resize(100, 50);
    target.setMouseTracking(true);
    target.show();
    QCoreApplication::processEvents();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&target, true);
    CHECK(components.supported);
    CHECK(components.input != nullptr);

    // Deliberately do not process the GUI event loop while producing this burst. A one-invocation-
    // per-packet implementation would queue 10k Qt events here. The bounded sink instead keeps one
    // drain invocation and collapses adjacent motion to the freshest coordinate.
    for (int i = 0; i < 10000; ++i) {
        hyremote::InputEvent move;
        move.kind = hyremote::InputEventKind::PointerMove;
        move.sourceViewport = {100U, 50U, 1.0F};
        move.x = static_cast<float>(i % 100);
        move.y = static_cast<float>(i % 50);
        components.input->post(move);
    }

    CHECK(target.moves == 0);
    CHECK(pumpUntil([&] { return target.moves != 0; }));
    CHECK(target.moves == 1);
    CHECK(std::fabs(target.lastPosition.x() - 99.0) <= 1.0);
    CHECK(std::fabs(target.lastPosition.y() - 49.0) <= 1.0);

    components.input.reset();
}

void testProtectedReleaseSurvivesNormalMailboxSaturation()
{
    HyRemote::detail::resetFactories();

    ProbeWidget target;
    target.resize(100, 50);
    target.setFocusPolicy(Qt::StrongFocus);
    target.show();
    target.setFocus();
    QCoreApplication::processEvents();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&target, true);
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

    CHECK(pumpUntil([&] { return target.buttonPresses == 1 && target.keyPresses == 1; }));

    // Saturate the normal 64-event budget without processing the GUI queue. These committed-text
    // events do not alter logical held state but reproduce the condition that previously rejected
    // disconnect cleanup releases with "bounded Qt input mailbox is full".
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

    // A new press cannot enter the saturated normal lane. Its later release must be recognized as
    // unmatched by adapter admission and dropped without consuming the protected release reserve.
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

    CHECK(pumpUntil([&] { return target.buttonReleases == 1 && target.keyReleases == 1; }));
    CHECK(target.buttonPresses == 1);
    CHECK(target.buttonReleases == 1);
    CHECK(target.keyPresses == 1);   // rejected B never reached QWidget
    CHECK(target.keyReleases == 1); // only the accepted Shift lifecycle was released

    components.input.reset();
}

void testShutdownBalancesDeliveredStateAndDropsPendingInput()
{
    HyRemote::detail::resetFactories();

    ProbeWidget target;
    target.resize(100, 50);
    target.setFocusPolicy(Qt::StrongFocus);
    target.show();
    target.setFocus();
    QCoreApplication::processEvents();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&target, true);
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

    CHECK(pumpUntil([&] { return target.buttonPresses == 1 && target.keyPresses == 1; }));
    CHECK(target.buttonReleases == 0);
    CHECK(target.keyReleases == 0);

    // This key is accepted into the adapter mailbox but deliberately not allowed to reach the GUI.
    // shutdown() must discard it, then balance only the button/Shift that were already delivered.
    hyremote::InputEvent pendingKey;
    pendingKey.kind = hyremote::InputEventKind::Key;
    pendingKey.key = hyremote::KeyCode::A;
    pendingKey.pressed = true;
    pendingKey.modifiers = hyremote::modifierMask(hyremote::InputModifier::Shift);
    components.input->post(pendingKey);

    components.input->shutdown();
    CHECK(target.buttonReleases == 1);
    CHECK(target.keyReleases == 1);
    CHECK(target.keyPresses == 1);  // pending A never reached QWidget

    QCoreApplication::processEvents();
    CHECK(target.buttonReleases == 1);
    CHECK(target.keyReleases == 1);
    CHECK(target.keyPresses == 1);

    // Destruction after terminal shutdown must not synthesize a second release sequence.
    components.input.reset();
    QCoreApplication::processEvents();
    CHECK(target.buttonReleases == 1);
    CHECK(target.keyReleases == 1);
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    testPointerFloodCoalescesBeforeGuiDelivery();
    testProtectedReleaseSurvivesNormalMailboxSaturation();
    testShutdownBalancesDeliveredStateAndDropsPendingInput();
    HyRemote::detail::resetFactories();
    if (failures != 0)
        std::cerr << failures << " Widgets input-backpressure checks failed\n";
    return failures == 0 ? 0 : 1;
}
