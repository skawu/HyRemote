// #401 deterministic reproduction of #400 through the real production Qt Quick input path.
//
// Same discipline as the Widgets preflight: drive the shipped path (InputSink::post -> bounded
// mailbox -> GUI drain -> Quick adapter -> QQuickWindow), record what Qt Quick actually receives,
// and classify the first divergence layer (#401 vocabulary). Assertions pin CURRENT observed
// behaviour; DIVERGENCE lines are the evidence #400's production fix will flip after the
// architecture decision.
//
// The Quick route already delegates item routing, hover, grab and focus to Qt Quick by delivering
// to the window, so the interesting Quick findings are: is onDoubleClicked reachable at all, and
// does the popup/overlay surface (which lives inside the same window) behave differently from the
// Widgets popup case.

#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QGuiApplication>
#include <QMouseEvent>
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
             bool pressed = true,
             hyremote::KeyCode key = hyremote::KeyCode::Unknown,
             const char *text = nullptr)
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
    event.key = key;
    if (text)
        event.textUtf8 = text;
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

void doubleClick(HyRemote::detail::TargetComponents &components,
                 const QQuickWindow &window,
                 const QPointF &windowPoint)
{
    postRaw(components, window, hyremote::InputEventKind::PointerButton, windowPoint,
            hyremote::PointerButton::Left, true);
    postRaw(components, window, hyremote::InputEventKind::PointerButton, windowPoint,
            hyremote::PointerButton::Left, false);
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

    MouseArea {
        objectName: "dblArea"
        x: 0; y: 0; width: 200; height: 200
        onPressed: bridge.inc("areaPresses")
        onClicked: bridge.inc("areaClicks")
        onDoubleClicked: bridge.inc("areaDoubleClicks")
    }

    Button {
        objectName: "button"
        x: 250; y: 20
        text: "ok"
        onClicked: bridge.inc("buttonClicks")
    }

    CheckBox {
        objectName: "checkBox"
        x: 250; y: 90
        text: "pick"
        onToggled: bridge.inc("toggles")
    }

    TextInput {
        objectName: "textInput"
        id: input
        x: 250; y: 160; width: 150; height: 30
        font.pixelSize: 16
        color: "black"
    }

    Slider {
        objectName: "slider"
        x: 0; y: 230; width: 300
        onMoved: bridge.inc("sliderMoves")
    }

    Popup {
        id: popup
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
        x: 0; y: 300
        text: "open"
        onClicked: popup.open()
    }
}
)QML",
                     QUrl(QStringLiteral("test_quick_input_preflight.qml")));
    QObject *qmlRoot = component.create();
    check(qmlRoot != nullptr, "qml: the inline matrix scene instantiates");
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

    std::cout << "OBSERVED host state: windowActive=" << (window.isActive() ? "true" : "false")
              << " platform=" << QGuiApplication::platformName().toStdString() << '\n';

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, true);
    check(components.input != nullptr, "quick input component is available");
    if (!components.input) {
        delete qmlRoot;
        return;
    }

    // 1. Single click: Qt Quick owns item routing already.
    click(components, window, QPointF(100, 100));
    check(bridge.property("areaClicks").toInt() == 1,
          "MouseArea: single remote click reaches onClicked");
    noDivergence("Quick single click / item routing",
                 "Qt Quick resolves the item under the point itself; the adapter only delivers to the window");

    // 2. Valid double click: is onDoubleClicked reachable on this path?
    doubleClick(components, window, QPointF(100, 100));
    const int doubleClicks = bridge.property("areaDoubleClicks").toInt();
    if (doubleClicks == 0) {
        divergence("QT_SEMANTIC_CLASSIFICATION",
                   "MouseArea onDoubleClicked",
                   "no DblClick event is ever delivered into the window, so MouseArea/TapHandler/tap "
                   "double-tap handlers are unreachable for remote users even though Qt Quick routing "
                   "itself is fine");
    } else {
        noDivergence("MouseArea onDoubleClicked", "a double-click semantic was observable");
    }

    // 3. Button and CheckBox single-click behaviour (control-local semantics work).
    //    Click points are derived from each item's own scene geometry instead of hand-computed
    //    coordinates, so a failure means input did not arrive, not that the fixture guessed wrong.
    const auto clickItem = [&components, &window, qmlRootItem](const char *objectName) {
        QQuickItem *item = qmlRootItem->findChild<QQuickItem *>(QString::fromLatin1(objectName));
        if (!item)
            item = window.findChild<QQuickItem *>(QString::fromLatin1(objectName));
        if (!item) {
            std::cout << "     item '" << objectName << "' not found; scene items:";
            const QList<QQuickItem *> all = qmlRootItem->findChildren<QQuickItem *>();
            for (QQuickItem *candidate : all)
                std::cout << ' ' << candidate->objectName().toStdString();
            std::cout << '\n';
            return false;
        }
        const QPointF center = item->mapToScene(QPointF(item->width() / 2.0, item->height() / 2.0));
        std::cout << "     item '" << objectName << "': scene center=(" << center.x() << ", " << center.y()
                  << ") size=" << item->width() << "x" << item->height()
                  << " visible=" << (item->isVisible() ? "true" : "false") << '\n';
        click(components, window, center);
        return true;
    };

    clickItem("button");
    pump();
    check(bridge.property("buttonClicks").toInt() == 1, "Button: single remote click activates");
    clickItem("checkBox");
    pump();
    check(bridge.property("toggles").toInt() == 1, "CheckBox: single remote click toggles");

    // 4. Focusable text target.
    clickItem("textInput");
    pump();
    QQuickItem *inputItem = window.findChild<QQuickItem *>(QStringLiteral("textInput"));
    const bool focused = inputItem && inputItem->hasActiveFocus();
    std::cout << "     observed[TextInput focus]: hasActiveFocus=" << (focused ? "true" : "false") << '\n';
    if (focused) {
        postRaw(components, window, hyremote::InputEventKind::Text, QPointF(0, 0),
                hyremote::PointerButton::None, true, hyremote::KeyCode::Unknown, "hy");
        pump();
        check(inputItem->property("text").toString().contains(QLatin1String("hy")),
              "TextInput: committed text reaches the focused field");
        noDivergence("Quick focus + text", "focus transition and text commit work");
    } else {
        divergence("OS_ACTIVATION_FOCUS/UNMEASURED",
                   "Quick TextInput focus",
                   "active focus was not granted in this offscreen harness even though the window reports "
                   "active; focus fidelity needs a real desktop run and is listed as an explicit gap, not "
                   "as a reproduced #400 defect");
    }

    // 5. Drag target.
    postRaw(components, window, hyremote::InputEventKind::PointerMove, QPointF(30, 250));
    postRaw(components, window, hyremote::InputEventKind::PointerButton, QPointF(30, 250),
            hyremote::PointerButton::Left, true);
    pump();
    postRaw(components, window, hyremote::InputEventKind::PointerMove, QPointF(280, 250));
    pump();
    postRaw(components, window, hyremote::InputEventKind::PointerButton, QPointF(280, 250),
            hyremote::PointerButton::Left, false);
    pump();
    check(bridge.property("sliderMoves").toInt() >= 1, "Slider: press/drag/release moves the value");
    noDivergence("Quick drag", "press/move/release reaches the dragged item");

    // 6. In-window popup/overlay surface: unlike the Widgets popup (separate top-level window),
    //    a Qt Quick Popup lives in the same window's overlay, so it should remain reachable.
    clickItem("openPopup");
    pump();
    const bool popupOpen = bridge.property("popupOpens").toInt() >= 1;
    std::cout << "     observed[Popup open]: onOpened fired = " << (popupOpen ? "true" : "false") << '\n';
    if (popupOpen) {
        clickItem("popupArea");
        pump();
        const int popupClicks = bridge.property("popupClicks").toInt();
        if (popupClicks >= 1) {
            noDivergence("Quick Popup/overlay input",
                         "in-window overlay surfaces stay reachable on the window-delivery route, unlike "
                         "the Widgets popup top-level");
        } else {
            divergence("TARGET_ROUTING",
                       "Quick Popup/overlay input",
                       "the popup opened but the click inside it did not reach the popup content");
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

    std::cout << (failures == 0 ? "PASS: quick input preflight reproduction"
                                : "FAIL: quick input preflight reproduction")
              << " (checks failed: " << failures << ", divergence scenarios: " << divergences << ")\n";
    return failures == 0 ? 0 : 1;
}

#include "test_quick_input_preflight.moc"
