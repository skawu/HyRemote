#include "../interactive_composite_target.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

#include <atomic>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include "hyremote/core/input.hpp"

namespace {

struct RecordedInput
{
    std::vector<hyremote::InputEvent> events;
    int shutdownCalls = 0;
};

class RecordingSink final : public hyremote::InputSink
{
public:
    explicit RecordingSink(std::shared_ptr<RecordedInput> recorded)
        : m_recorded(std::move(recorded))
    {
    }

    void post(const hyremote::InputEvent &event) override
    {
        if (m_failNextPost.exchange(false)) {
            // Models a child adapter that rejects one event; the composite must treat this as a deferred error
            // rather than a delivery.
            throw std::runtime_error("recording sink rejected one event");
        }
        m_recorded->events.push_back(event);
    }

    void shutdown() noexcept override { ++m_recorded->shutdownCalls; }

    void failNextPost() { m_failNextPost.store(true); }

private:
    std::shared_ptr<RecordedInput> m_recorded;
    std::atomic<bool> m_failNextPost{false};
};

bool check(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool waitForCount(const std::shared_ptr<RecordedInput> &left,
                  const std::shared_ptr<RecordedInput> &right,
                  std::size_t count)
{
    QElapsedTimer timer;
    timer.start();
    while (left->events.size() + right->events.size() < count && timer.elapsed() < 2000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return left->events.size() + right->events.size() >= count;
}

void drainEvents(int milliseconds = 100)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

hyremote::InputEvent pointerEvent(hyremote::InputEventKind kind,
                                  float x,
                                  float y,
                                  hyremote::PointerButton button = hyremote::PointerButton::None,
                                  bool pressed = false)
{
    hyremote::InputEvent event;
    event.kind = kind;
    event.sourceViewport = {150, 100, 1.0F};
    event.x = x;
    event.y = y;
    event.button = button;
    event.pressed = pressed;
    return event;
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QObject leftTarget;
    QObject rightTarget;
    auto left = std::make_shared<RecordedInput>();
    auto right = std::make_shared<RecordedInput>();

    HyRemote::Qpa::InteractiveCompositeTarget composite;
    // Canvas = [-50, 0] .. [99, 99]. Surfaces overlap for global x=0..49; right is topmost.
    composite.upsertSurface(1, &leftTarget, QRect(-50, 0, 100, 100), true);
    composite.upsertSurface(2, &rightTarget, QRect(0, 0, 100, 100), true);

    // Held by the test as well as by the composite, so a case can make one child adapter reject an event.
    auto leftSink = std::make_shared<RecordingSink>(left);
    auto rightSink = std::make_shared<RecordingSink>(right);

    const HyRemote::detail::BuiltinTargetResolver resolver =
        [&](QObject *target, bool remoteInputEnabled) {
            HyRemote::detail::TargetComponents result;
            if (!remoteInputEnabled)
                return result;
            if (target == &leftTarget) {
                result.supported = true;
                result.input = leftSink;
            } else if (target == &rightTarget) {
                result.supported = true;
                result.input = rightSink;
            }
            return result;
        };

    HyRemote::detail::TargetComponents components = composite.createTargetComponents(true, resolver);
    if (!check(components.supported && components.input != nullptr,
               "interactive composite target supplies one input sink")) {
        return 1;
    }

    // Remote canvas x=60 maps to global x=10, inside both surfaces. Newer/right surface wins.
    components.input->post(pointerEvent(hyremote::InputEventKind::PointerMove, 60, 20));
    if (!check(waitForCount(left, right, 1), "topmost pointer event is delivered")
        || !check(left->events.empty(), "lower overlapping surface does not receive pointer")
        || !check(right->events.size() == 1, "topmost overlapping surface receives pointer")
        || !check(right->events.back().sourceViewport.width == 100
                      && right->events.back().sourceViewport.height == 100,
                  "child pointer viewport is rewritten to surface-local logical geometry")
        || !check(right->events.back().x == 10.0F && right->events.back().y == 20.0F,
                  "canvas pointer is translated to right-surface local coordinates")) {
        return 2;
    }

    // Press in the overlap establishes a surface grab on the right surface.
    components.input->post(pointerEvent(hyremote::InputEventKind::PointerButton,
                                        60,
                                        20,
                                        hyremote::PointerButton::Left,
                                        true));
    if (!check(waitForCount(left, right, 2), "pointer press is delivered")
        || !check(right->events.size() == 2, "press stays on right surface")) {
        return 3;
    }

    // x=10 maps to global x=-40, which normally belongs only to the left surface. During a drag,
    // move/release must remain with the press owner so one child sink owns the full button lifecycle.
    components.input->post(pointerEvent(hyremote::InputEventKind::PointerMove, 10, 20));
    components.input->post(pointerEvent(hyremote::InputEventKind::PointerButton,
                                        10,
                                        20,
                                        hyremote::PointerButton::Left,
                                        false));
    if (!check(waitForCount(left, right, 4), "drag move/release are delivered")
        || !check(left->events.empty(), "grab prevents drag lifecycle from switching surfaces")
        || !check(right->events.size() == 4, "press owner receives move and release")
        || !check(right->events[2].x == -40.0F,
                  "grabbed surface receives coordinates relative to its own geometry even outside bounds")) {
        return 4;
    }

    // After release the grab is gone, so the same x=10 point routes to the left surface.
    components.input->post(pointerEvent(hyremote::InputEventKind::PointerMove, 10, 20));
    if (!check(waitForCount(left, right, 5), "post-release pointer is delivered")
        || !check(left->events.size() == 1, "post-release pointer re-evaluates topmost surface")
        || !check(left->events.back().x == 10.0F && left->events.back().y == 20.0F,
                  "left-surface local coordinates are correct")) {
        return 5;
    }

    composite.setActiveSurface(1);
    hyremote::InputEvent key;
    key.kind = hyremote::InputEventKind::Key;
    key.key = hyremote::KeyCode::A;
    key.pressed = true;
    components.input->post(key);
    if (!check(waitForCount(left, right, 6), "key event is delivered")
        || !check(left->events.size() == 2 && left->events.back().kind == hyremote::InputEventKind::Key,
                  "key event follows explicit active application surface")) {
        return 6;
    }

    hyremote::InputEvent text;
    text.kind = hyremote::InputEventKind::Text;
    text.textUtf8 = "hello";
    components.input->post(text);
    if (!check(waitForCount(left, right, 7), "text event is delivered")
        || !check(left->events.size() == 3 && left->events.back().textUtf8 == "hello",
                  "text event follows the active surface without protocol-specific focus rules")) {
        return 7;
    }

    // Stacking order is not keyboard focus. Once the active surface is hidden, key/text events are
    // intentionally dropped until the QPA controller reports a new real Qt active/focus surface.
    // Pruning that surface must terminally shut down its child sink first, so any Qt-facing child
    // adapter can balance remote state it already delivered before leaving the application canvas.
    composite.setSurfaceVisible(1, false);
    components.input->post(key);
    drainEvents();
    if (!check(left->events.size() + right->events.size() == 7,
               "hidden active surface does not invent a topmost keyboard target")
        || !check(left->shutdownCalls == 1,
                  "hidden/pruned child input is shut down before its adapter is released")) {
        return 8;
    }

    composite.setActiveSurface(2);
    components.input->post(key);
    if (!check(waitForCount(left, right, 8), "new explicit active surface receives key")
        || !check(right->events.size() == 5 && right->events.back().kind == hyremote::InputEventKind::Key,
                  "keyboard routing resumes only after explicit active-surface update")) {
        return 9;
    }

    // #164 acceptance 3, deterministic multi-child failure: a child adapter that rejects an event must not leave
    // that event half-delivered, the failure must surface on the next post() with that call's own event rejected,
    // and the composite must stay usable afterwards.
    const std::size_t leftBeforeFailure = left->events.size();
    const std::size_t rightBeforeFailure = right->events.size();

    rightSink->failNextPost();
    components.input->post(pointerEvent(hyremote::InputEventKind::PointerMove, 60, 20));
    drainEvents(200);
    if (!check(right->events.size() == rightBeforeFailure,
               "a rejected child event is not recorded as delivered")
        || !check(left->events.size() == leftBeforeFailure,
                  "a rejected child event does not fall through to another child")) {
        return 12;
    }

    bool deferredFailureReported = false;
    std::string deferredFailureMessage;
    try {
        components.input->post(pointerEvent(hyremote::InputEventKind::PointerMove, 70, 25));
    } catch (const std::exception &error) {
        deferredFailureReported = true;
        deferredFailureMessage = error.what();
    }
    drainEvents(200);
    if (!check(deferredFailureReported,
               "the deferred child failure surfaces on the next post instead of being swallowed")
        || !check(deferredFailureMessage.find("child input adapter rejected an event") != std::string::npos,
                  "the deferred failure names the rejecting child adapter")
        || !check(left->events.size() == leftBeforeFailure && right->events.size() == rightBeforeFailure,
                  "the post that reports the deferred failure does not accept its own event")) {
        return 13;
    }

    components.input->post(pointerEvent(hyremote::InputEventKind::PointerMove, 60, 20));
    if (!check(waitForCount(left, right, leftBeforeFailure + rightBeforeFailure + 1),
               "the composite accepts input again once the deferred failure has been reported")
        || !check(right->events.size() == rightBeforeFailure + 1,
                  "the recovery delivery reaches the previously failing child exactly once")) {
        return 14;
    }

    components.input->shutdown();
    if (!check(left->shutdownCalls == 1,
               "already-pruned child is not shut down twice by composite teardown")
        || !check(right->shutdownCalls == 1,
                  "composite terminal shutdown propagates to the remaining child sink")) {
        return 10;
    }
    components.input->shutdown();
    if (!check(right->shutdownCalls == 1,
               "composite terminal shutdown is idempotent")) {
        return 11;
    }

    std::cout << "PASS: QPA composite input routes lifecycle and propagates terminal shutdown correctly\n";
    return 0;
}
