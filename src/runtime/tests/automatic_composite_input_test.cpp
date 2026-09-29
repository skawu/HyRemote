#include "automatic/interactive_composite_target.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "detail/qt_window_system_input.hpp"
#include "hyremote/core/input.hpp"

namespace {

struct RecordedInput
{
    std::vector<hyremote::InputEvent> events;
    std::vector<unsigned long> acceptedTimestamps;
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
        m_recorded->events.push_back(event);
        m_recorded->acceptedTimestamps.push_back(HyRemote::detail::qtWindowSystemTimestamp());
    }
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

    // #406 architecture applies before surface routing too: pointer motion is semantic input to Qt.
    // Queue several moves without allowing the GUI drain and prove none is coalesced or evicted.
    const std::size_t beforeLossless = right->events.size();
    components.input->post(pointerMove(61, 20));
    components.input->post(pointerMove(62, 20));
    components.input->post(pointerMove(63, 20));
    if (!check(waitForTotal(left, right, 2 + beforeLossless + 3 - 1),
               "all queued pointer moves reach the selected surface")) {
        return 5;
    }
    if (!check(right->events.size() == beforeLossless + 3,
               "composite input does not coalesce semantic pointer history")) {
        return 6;
    }

    // The child is invoked only when the GUI drains, but its timestamp must still reflect when the
    // composite accepted each fact. Without the Runtime-private accepted-timestamp scope these two
    // observations collapse to nearly the same drain time.
    const std::size_t timestampBase = right->acceptedTimestamps.size();
    components.input->post(pointerMove(64, 20));
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    components.input->post(pointerMove(65, 20));
    if (!check(waitForTotal(left, right, left->events.size() + beforeLossless + 5),
               "timestamp qualification moves are delivered")) {
        return 7;
    }
    if (!check(right->acceptedTimestamps.size() >= timestampBase + 2,
               "child recorded both accepted timestamps")) {
        return 8;
    }
    const unsigned long firstTimestamp = right->acceptedTimestamps[timestampBase];
    const unsigned long secondTimestamp = right->acceptedTimestamps[timestampBase + 1];
    if (!check(secondTimestamp >= firstTimestamp + 20,
               "GUI drain delay does not replace original composite acceptance spacing")) {
        return 9;
    }

    // The normal lane remains bounded. Saturation is an explicit rejection, never a stale-move
    // deletion. Drain afterwards so shutdown is exercised from a clean pending state.
    bool rejected = false;
    for (int i = 0; i < 64; ++i)
        components.input->post(pointerMove(70.0F + static_cast<float>(i % 20), 25));
    try {
        components.input->post(pointerMove(90, 25));
    } catch (const std::runtime_error &) {
        rejected = true;
    }
    if (!check(rejected, "65th pending normal composite event applies backpressure"))
        return 10;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

    components.input->shutdown();
    if (!check(right->shutdownCalls == 1, "remaining child input is shut down on terminal stop"))
        return 11;

    std::cout << "PASS: automatic runtime routes pointer/keyboard across surfaces without losing pointer history\n";
    return 0;
}
