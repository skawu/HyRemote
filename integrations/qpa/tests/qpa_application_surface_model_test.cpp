// SPDX-License-Identifier: Apache-2.0
#include "../application_surface_model.hpp"

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
    using HyRemote::Qpa::ApplicationSurfaceModel;

    ApplicationSurfaceModel model;
    if (!check(model.canvasBounds().isEmpty(), "empty model has no canvas"))
        return 1;

    // Primary window spans negative global coordinates. Dialog overlaps it and extends right/bottom.
    model.upsert(1, QRect(-200, -100, 640, 480), true);
    model.upsert(2, QRect(100, 50, 320, 240), true);
    if (!check(model.canvasBounds() == QRect(-200, -100, 640, 480),
               "contained dialog does not enlarge canvas")) {
        return 2;
    }

    auto routed = model.routeCanvasPoint(QPoint(350, 200));  // global (150,100), overlap area
    if (!check(routed.has_value(), "overlap point routes")
        || !check(routed->surfaceId == 2, "newly visible dialog is topmost")
        || !check(routed->localPosition == QPoint(50, 50), "dialog local coordinate is mapped")) {
        return 3;
    }

    model.raise(1);
    routed = model.routeCanvasPoint(QPoint(350, 200));
    if (!check(routed.has_value() && routed->surfaceId == 1,
               "raise changes overlap routing without changing canvas")) {
        return 4;
    }

    // Move dialog outside the primary window so the union expands in both positive axes.
    model.setGeometry(2, QRect(500, 400, 300, 200));
    if (!check(model.canvasBounds() == QRect(-200, -100, 1000, 700),
               "canvas is the union of visible surface bounds")) {
        return 5;
    }
    routed = model.routeCanvasPoint(QPoint(750, 550));  // global (550,450)
    if (!check(routed.has_value() && routed->surfaceId == 2,
               "expanded canvas point routes to moved dialog")
        || !check(routed->localPosition == QPoint(50, 50), "moved dialog local coordinate is mapped")) {
        return 6;
    }

    // A gap in the rectangular canvas must not be routed to an unrelated surface.
    if (!check(!model.routeCanvasPoint(QPoint(680, 200)).has_value(),
               "canvas gaps remain non-interactive")) {
        return 7;
    }

    model.setVisible(2, false);
    if (!check(model.canvasBounds() == QRect(-200, -100, 640, 480),
               "hidden surface is removed from canvas bounds")) {
        return 8;
    }

    model.setVisible(2, true);  // becoming visible raises it by policy.
    routed = model.routeCanvasPoint(QPoint(750, 550));
    if (!check(routed.has_value() && routed->surfaceId == 2,
               "re-shown surface returns at the front")) {
        return 9;
    }

    model.remove(2);
    if (!check(!model.contains(2), "removed surface leaves the model")
        || !check(model.canvasBounds() == QRect(-200, -100, 640, 480),
                  "removal restores remaining canvas")) {
        return 10;
    }

    // Repeated geometry churn must not mutate stacking unless visibility/raise semantics say so.
    model.upsert(3, QRect(-150, -50, 100, 100), true);
    const auto before = model.visibleBackToFront();
    for (int i = 0; i < 1000; ++i)
        model.setGeometry(1, QRect(-200 + (i % 3), -100, 640, 480));
    const auto after = model.visibleBackToFront();
    if (!check(before.size() == after.size(), "geometry churn preserves surface count")
        || !check(before.back().id == after.back().id, "geometry churn preserves topmost surface")) {
        return 11;
    }

    std::cout << "PASS: QPA application surface canvas/stack/routing model\n";
    return 0;
}
