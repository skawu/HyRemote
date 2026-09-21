#include "automatic/interactive_composite_target.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

#include <iostream>
#include <memory>
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

    void post(const hyremote::InputEvent &event) override { m_recorded->events.push_back(event); }
    void shutdown() noexcept override { ++m_recorded->shutdownCalls; }

private:
    std::shared_ptr<RecordedInput> m_recorded;
};

bool check(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool waitForTotal(const std::shared_ptr<RecordedInput> &left,
                  const std::shared_ptr<RecordedInput> &right,
                  std::size_t count)
{
    QElapsedTimer timer;
    timer.start();
    while (left->events.size() + right->events.size() < count && timer.elapsed() < 2000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return left->events.size() + right->events.size() >= count;
}

hyremote::InputEvent pointerMove(float x, float y)
{
    hyremote::InputEvent event;
    event.kind = hyremote::InputEventKind::PointerMove;
    event.sourceViewport = {150, 100, 1.0F};
    event.x = x;
    event.y = y;
    return event;
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    using HyRemote::Runtime::Automatic::InteractiveCompositeTarget;

    QObject leftTarget;
    QObject rightTarget;
    auto left = std::make_shared<RecordedInput>();
    auto right = std::make_shared<RecordedInput>();

    InteractiveCompositeTarget composite;
    composite.upsertSurface(1, &leftTarget, QRect(-50, 0, 100, 100), true);
    composite.upsertSurface(2, &rightTarget, QRect(0, 0, 100, 100), true);

    const HyRemote::detail::BuiltinTargetResolver resolver =
        [&](QObject *target, bool remoteInputEnabled) {
            HyRemote::detail::TargetComponents result;
            if (!remoteInputEnabled)
                return result;
            if (target == &leftTarget) {
                result.supported = true;
                result.input = std::make_shared<RecordingSink>(left);
            } else if (target == &rightTarget) {
                result.supported = true;
                result.input = std::make_shared<RecordingSink>(right);
            }
            return result;
        };

    auto components = composite.createTargetComponents(true, resolver);
    if (!check(components.supported && components.input, "automatic composite creates input sink"))
        return 1;

    // Canvas x=60 maps to global x=10, inside both surfaces; the newer/right surface is topmost.
    components.input->post(pointerMove(60, 20));
    if (!check(waitForTotal(left, right, 1), "pointer is delivered")
        || !check(left->events.empty(), "lower overlapping surface does not receive pointer")
        || !check(right->events.size() == 1, "topmost surface receives pointer")
        || !check(right->events.front().x == 10.0F && right->events.front().y == 20.0F,
                  "pointer is rewritten to surface-local coordinates")) {
        return 2;
    }

    composite.setActiveSurface(1);
    hyremote::InputEvent key;
    key.kind = hyremote::InputEventKind::Key;
    key.key = hyremote::KeyCode::A;
    key.pressed = true;
    components.input->post(key);
    if (!check(waitForTotal(left, right, 2), "key is delivered")
        || !check(left->events.size() == 1 && left->events.front().kind == hyremote::InputEventKind::Key,
                  "keyboard follows explicit active application surface")) {
        return 3;
    }

    composite.setSurfaceVisible(1, false);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    components.input->post(key);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    if (!check(left->events.size() == 1 && right->events.size() == 1,
               "hidden active surface does not invent a keyboard target")) {
        return 4;
    }

    components.input->shutdown();
    if (!check(right->shutdownCalls == 1, "remaining child input is shut down on terminal stop"))
        return 5;

    std::cout << "PASS: automatic runtime routes pointer and keyboard input across application surfaces\n";
    return 0;
}
