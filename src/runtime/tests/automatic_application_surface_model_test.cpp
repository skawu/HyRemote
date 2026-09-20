#include "automatic/application_surface_model.hpp"

#include <QPoint>
#include <QRect>

#include <iostream>

namespace {

bool check(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

}  // namespace

int main()
{
    using HyRemote::Runtime::Automatic::ApplicationSurfaceModel;

    ApplicationSurfaceModel model;
    if (!check(model.canvasBounds().isEmpty(), "empty automatic model has no canvas"))
        return 1;

    model.upsert(1, QRect(-200, -100, 640, 480), true);
    model.upsert(2, QRect(100, 50, 320, 240), true);
    if (!check(model.canvasBounds() == QRect(-200, -100, 640, 480),
               "contained surface does not enlarge canvas")) {
        return 2;
    }

    auto routed = model.routeCanvasPoint(QPoint(350, 200));
    if (!check(routed.has_value(), "overlap point routes")
        || !check(routed->surfaceId == 2, "newly visible surface is topmost")
        || !check(routed->localPosition == QPoint(50, 50), "local coordinate is mapped")) {
        return 3;
    }

    model.raise(1);
    routed = model.routeCanvasPoint(QPoint(350, 200));
    if (!check(routed.has_value() && routed->surfaceId == 1,
               "raise changes overlap routing without changing canvas")) {
        return 4;
    }

    model.setGeometry(2, QRect(500, 400, 300, 200));
    if (!check(model.canvasBounds() == QRect(-200, -100, 1000, 700),
               "canvas follows visible surface union")) {
        return 5;
    }

    if (!check(!model.routeCanvasPoint(QPoint(680, 200)).has_value(),
               "rectangular canvas gaps remain non-interactive")) {
        return 6;
    }

    model.setVisible(2, false);
    if (!check(model.canvasBounds() == QRect(-200, -100, 640, 480),
               "hidden surface leaves canvas bounds")) {
        return 7;
    }

    model.remove(2);
    if (!check(!model.contains(2), "removed surface leaves model"))
        return 8;

    std::cout << "PASS: automatic application-surface model owns canvas/stack/routing semantics\n";
    return 0;
}
