#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QTimer>

#include <cmath>
#include <functional>
#include <iostream>

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
    QPointF lastPosition;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched);
        if (event->type() == QEvent::MouseMove) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            ++moves;
            lastPosition = mouse->position();
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

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    testPointerFloodCoalescesBeforeGuiDelivery();
    HyRemote::detail::resetFactories();
    if (failures != 0)
        std::cerr << failures << " Quick input-backpressure checks failed\n";
    return failures == 0 ? 0 : 1;
}
