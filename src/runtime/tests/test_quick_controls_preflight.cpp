// #401 deterministic reproduction for the QtQuick.Controls control matrix.
//
// Split from test_quick_input_preflight.cpp on purpose: the Runtime's Quick capability contract
// (HYREMOTE_REMOTEACCESS_WITH_QUICK) promises Qt Quick, not the QtQuick.Controls QML module, so the
// baseline preflight must not depend on Controls and a minimal Quick-only configuration must stay
// valid. This fixture is registered only when the QtQuick.Controls module is actually available
// (see src/runtime/tests/CMakeLists.txt), which is a real capability guard rather than a runtime
// skip: an unavailable capability leaves no broken or unbuildable test in the graph.
//
// The Popup reachability result is asserted, not printed: #401 claims that an in-window Quick
// overlay stays reachable, and a claim like that has to be regression-bearing.

#include <QEventLoop>
#include <QGuiApplication>
#include <QObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStyleHints>

#include <iostream>

#include "detail/component_factories.hpp"
#include "hyremote/core/input.hpp"

namespace {

int failures = 0;
int divergences = 0;

void check(bool ok, const char *what)
{
    if (!ok) {
        std::cerr << "CHECK failed: " << what << '\n';
        ++failures;
    }
    std::cout << (ok ? "ok   " : "FAIL ") << what << '\n';
}

void divergence(const char *layer, const char *scenario, const char *detail)
{
    ++divergences;
    std::cout << "DIVERGENCE[" << layer << "] " << scenario << ": " << detail << '\n';
}

void noDivergence(const char *scenario, const char *detail)
{
    std::cout << "NO_DEFECT " << scenario << ": " << detail << '\n';
}

void pump()
{
    for (int i = 0; i < 30; ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

class Bridge final : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    Q_INVOKABLE void inc(const QString &name)
    {
        const QByteArray key = name.toUtf8();
        setProperty(key.constData(), property(key.constData()).toInt() + 1);
    }
};

void postRaw(HyRemote::detail::TargetComponents &components,
             const QQuickWindow &window,
             hyremote::InputEventKind kind,
             const QPointF &windowPoint,
             hyremote::PointerButton button = hyremote::PointerButton::None,
             bool pressed = true)
{
    hyremote::InputEvent event;
    event.kind = kind;
    event.sourceViewport.width = static_cast<std::uint32_t>(window.width());
    event.sourceViewport.height = static_cast<std::uint32_t>(window.height());
    event.sourceViewport.devicePixelRatio = 1.0F;
    event.x = static_cast<float>(windowPoint.x());
    event.y = static_cast<float>(windowPoint.y());
    event.button = button;
    event.pressed = pressed;
    components.input->post(event);
}

void click(HyRemote::detail::TargetComponents &components,
           const QQuickWindow &window,
           const QPointF &windowPoint)
{
    postRaw(components, window, hyremote::InputEventKind::PointerButton, windowPoint,
            hyremote::PointerButton::Left, true);
    postRaw(components, window, hyremote::InputEventKind::PointerButton, windowPoint,
            hyremote::PointerButton::Left, false);
    pump();
}

void runPreflight()
{
    HyRemote::detail::resetFactories();

    QQmlEngine engine;
    Bridge bridge;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QQmlComponent component(&engine);
    component.setData(R"QML(
import QtQuick
import QtQuick.Controls

Rectangle {
    width: 500
    height: 500
    color: "white"

    Button {
        objectName: "button"
        x: 0; y: 0
        text: "ok"
        onClicked: bridge.inc("buttonClicks")
    }

    CheckBox {
        objectName: "checkBox"
        x: 0; y: 80
        text: "pick"
        onToggled: bridge.inc("toggles")
    }

    Slider {
        objectName: "slider"
        x: 0; y: 160; width: 300
        onMoved: bridge.inc("sliderMoves")
    }

    Popup {
        id: popup
        objectName: "popup"
        x: 220; y: 300; width: 220; height: 160
        onOpened: bridge.inc("popupOpens")
        MouseArea {
            objectName: "popupArea"
            anchors.fill: parent
            onClicked: bridge.inc("popupClicks")
        }
    }

    Button {
        objectName: "openPopup"
        x: 0; y: 240
        text: "open"
        onClicked: popup.open()
    }
}
)QML",
                     QUrl(QStringLiteral("test_quick_controls_preflight.qml")));
    QObject *qmlRoot = component.create();
    check(qmlRoot != nullptr, "qml: the inline Controls matrix scene instantiates");
    if (!qmlRoot) {
        for (const QQmlError &error : component.errors())
            std::cerr << "  QML error: " << error.toString().toStdString() << '\n';
        return;
    }
    auto *qmlRootItem = qobject_cast<QQuickItem *>(qmlRoot);
    if (!qmlRootItem)
        return;

    QQuickWindow window;
    window.resize(500, 500);
    qmlRootItem->setParentItem(window.contentItem());
    window.show();
    pump();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, true);
    check(components.input != nullptr, "quick input component is available");
    if (!components.input) {
        delete qmlRoot;
        return;
    }

    // Click points come from each control's own scene geometry: the active Controls style decides
    // implicit sizes and internal layout, so hand-computed coordinates would measure the style.
    const auto sceneCenter = [qmlRootItem](const char *objectName) {
        QQuickItem *item = qmlRootItem->findChild<QQuickItem *>(QString::fromLatin1(objectName));
        if (!item) {
            std::cout << "     item '" << objectName << "' not found\n";
            return QPointF(-1, -1);
        }
        const QPointF center = item->mapToScene(QPointF(item->width() / 2.0, item->height() / 2.0));
        std::cout << "     item '" << objectName << "': scene center=(" << center.x() << ", "
                  << center.y() << ") size=" << item->width() << "x" << item->height()
                  << " visible=" << (item->isVisible() ? "true" : "false") << '\n';
        return center;
    };

    // Control-local activation.
    click(components, window, sceneCenter("button"));
    check(bridge.property("buttonClicks").toInt() == 1, "Button: single remote click activates");
    noDivergence("Quick Button", "control-local activation works");

    click(components, window, sceneCenter("checkBox"));
    check(bridge.property("toggles").toInt() == 1, "CheckBox: single remote click toggles");
    noDivergence("Quick CheckBox", "control-local toggle works");

    // Slider drag: observed and classified, because the style decides the groove/handle layout.
    QQuickItem *sliderItem = qmlRootItem->findChild<QQuickItem *>(QStringLiteral("slider"));
    if (sliderItem) {
        const double before = sliderItem->property("value").toDouble();
        const QPointF start =
            sliderItem->mapToScene(QPointF(sliderItem->width() * 0.05, sliderItem->height() / 2.0));
        const QPointF end =
            sliderItem->mapToScene(QPointF(sliderItem->width() * 0.95, sliderItem->height() / 2.0));
        postRaw(components, window, hyremote::InputEventKind::PointerMove, start);
        postRaw(components, window, hyremote::InputEventKind::PointerButton, start,
                hyremote::PointerButton::Left, true);
        pump();
        postRaw(components, window, hyremote::InputEventKind::PointerMove, end);
        pump();
        postRaw(components, window, hyremote::InputEventKind::PointerButton, end,
                hyremote::PointerButton::Left, false);
        pump();
        const double after = sliderItem->property("value").toDouble();
        std::cout << "     observed[Slider drag]: before=" << before << " after=" << after
                  << " onMoved=" << bridge.property("sliderMoves").toInt() << '\n';
        if (after > before) {
            noDivergence("Quick drag", "press/move/release moves the control's value");
        } else {
            divergence("TARGET_ROUTING/UNMEASURED",
                       "Quick Slider drag",
                       "the geometry-derived drag did not change the value in this offscreen harness; "
                       "drag fidelity needs a real desktop run and is listed as an explicit gap, not as a "
                       "reproduced #400 defect");
        }
    } else {
        divergence("TARGET_ROUTING/UNMEASURED", "Quick Slider drag", "slider item not found");
    }

    // Popup: a hard, regression-bearing claim - the remote path must open the in-window popup and
    // must reach its content, otherwise this fixture fails.
    click(components, window, sceneCenter("openPopup"));
    const bool popupOpen = bridge.property("popupOpens").toInt() >= 1;
    std::cout << "     observed[Popup open]: onOpened fired = " << (popupOpen ? "true" : "false") << '\n';
    check(popupOpen, "Popup: the remote click opens the in-window popup");

    if (popupOpen) {
        click(components, window, sceneCenter("popupArea"));
        const int popupClicks = bridge.property("popupClicks").toInt();
        std::cout << "     observed[Popup content]: popupClicks=" << popupClicks << '\n';
        check(popupClicks >= 1, "Popup: a remote click reaches the popup content");
        if (popupClicks >= 1) {
            noDivergence("Quick Popup/overlay input",
                         "in-window overlay surfaces stay reachable on the window-delivery route, unlike "
                         "the Widgets popup top-level owned by #404");
        }
    }

    components.input.reset();
    delete qmlRoot;
}

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (QStyleHints *hints = QGuiApplication::styleHints())
        hints->setMouseDoubleClickInterval(400);

    runPreflight();

    std::cout << (failures == 0 ? "PASS: quick controls preflight reproduction"
                                : "FAIL: quick controls preflight reproduction")
              << " (checks failed: " << failures << ", divergence scenarios: " << divergences << ")\n";
    return failures == 0 ? 0 : 1;
}

#include "test_quick_controls_preflight.moc"
