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

void testPointerFloodBackpressuresWithoutSemanticLoss()
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

    // Raw pointer history is Qt semantic input. The bounded Runtime therefore keeps every accepted
    // move instead of collapsing excursions and trying to reconstruct their meaning later.
    for (int i = 0; i < 64; ++i) {
        hyremote::InputEvent move;
        move.kind = hyremote::InputEventKind::PointerMove;
        move.sourceViewport = {100U, 50U, 1.0F};
        move.x = static_cast<float>(i);
        move.y = static_cast<float>(i % 50);
        components.input->post(move);
    }

    hyremote::InputEvent overflow;
    overflow.kind = hyremote::InputEventKind::PointerMove;
    overflow.sourceViewport = {100U, 50U, 1.0F};
    overflow.x = 64.0F;
    overflow.y = 14.0F;
    bool backpressured = false;
    try {
        components.input->post(overflow);
    } catch (const std::runtime_error &) {
        backpressured = true;
    }
    CHECK(backpressured);
    CHECK(target.moves == 0);

    CHECK(pumpUntil([&] { return target.moves == 64; }));
    CHECK(target.moves == 64);
    CHECK(std::fabs(target.lastPosition.x() - 63.0) <= 1.0);
    CHECK(std::fabs(target.lastPosition.y() - 13.0) <= 1.0);

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

    CHECK(pumpUntil([&] { return target.buttonReleases == 1 && target.keyReleases == 1; }));
    CHECK(target.buttonPresses == 1);
    CHECK(target.buttonReleases == 1);
    CHECK(target.keyPresses == 1);
    CHECK(target.keyReleases == 1);

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

    hyremote::InputEvent pendingKey;
    pendingKey.kind = hyremote::InputEventKind::Key;
    pendingKey.key = hyremote::KeyCode::A;
    pendingKey.pressed = true;
    pendingKey.modifiers = hyremote::modifierMask(hyremote::InputModifier::Shift);
    components.input->post(pendingKey);

    components.input->shutdown();
    CHECK(target.buttonReleases == 1);
    CHECK(target.keyReleases == 1);
    CHECK(target.keyPresses == 1);

    QCoreApplication::processEvents();
    CHECK(target.buttonReleases == 1);
    CHECK(target.keyReleases == 1);
    CHECK(target.keyPresses == 1);

    components.input.reset();
    QCoreApplication::processEvents();
    CHECK(target.buttonReleases == 1);
    CHECK(target.keyReleases == 1);
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    testPointerFloodBackpressuresWithoutSemanticLoss();
    testProtectedReleaseSurvivesNormalMailboxSaturation();
    testShutdownBalancesDeliveredStateAndDropsPendingInput();
    HyRemote::detail::resetFactories();
    if (failures != 0)
        std::cerr << failures << " Widgets input-backpressure checks failed\n";
    return failures == 0 ? 0 : 1;
}
