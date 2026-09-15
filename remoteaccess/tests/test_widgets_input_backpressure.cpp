#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QMouseEvent>
#include <QTimer>
#include <QWidget>

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

class ProbeWidget final : public QWidget
{
public:
    int moves = 0;
    QPointF lastPosition;

protected:
    void mouseMoveEvent(QMouseEvent *event) override
    {
        ++moves;
        lastPosition = event->position();
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

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    testPointerFloodCoalescesBeforeGuiDelivery();
    HyRemote::detail::resetFactories();
    if (failures != 0)
        std::cerr << failures << " Widgets input-backpressure checks failed\n";
    return failures == 0 ? 0 : 1;
}
