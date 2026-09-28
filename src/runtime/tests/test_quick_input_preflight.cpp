// #401 deterministic reproduction through the real production Qt Quick input path.
//
// Same discipline as the Widgets preflight: drive the shipped path (InputSink::post -> bounded
// mailbox -> GUI drain -> Quick adapter -> QQuickWindow), record what Qt Quick actually receives,
// and classify the first divergence layer (#401 vocabulary).
//
// This fixture deliberately depends on **QtQuick only**, never on QtQuick.Controls: the Runtime's
// Quick capability contract (HYREMOTE_REMOTEACCESS_WITH_QUICK) promises Qt Quick, so the baseline
// preflight must stay buildable and runnable in a minimal Quick-only configuration. The control
// matrix that needs QtQuick.Controls lives in test_quick_controls_preflight.cpp, which is only
// registered when that QML module is actually available.
//
// Evidence kept here (all QtQuick-native): MouseArea click, MouseArea double-click, focusable text
// target, drag with a held button, and an in-window overlay surface above the content.
//
// Assertions pin CURRENT observed behaviour. DIVERGENCE lines are measured evidence. The overlay
// reachability claim is a hard assertion on purpose: a "NO_DEFECT" statement about a reachable
// surface must be regression-bearing, not a printout.

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

Rectangle {
    width: 500
    height: 500
    color: "white"

    MouseArea {
        objectName: "area"
        x: 0; y: 0; width: 200; height: 200
        onClicked: bridge.inc("areaClicks")
        onDoubleClicked: bridge.inc("areaDoubleClicks")
    }

    TextInput {
        objectName: "textInput"
        id: input
        x: 0; y: 240; width: 200; height: 30
        font.pixelSize: 16
        color: "black"
    }

    // Drag target: a held press plus moves, counted by the item itself.
    MouseArea {
        objectName: "dragArea"
        x: 250; y: 240; width: 80; height: 80
        hoverEnabled: true
        onPressed: bridge.inc("dragPresses")
        onPositionChanged: bridge.inc("dragMoves")
    }

    // In-window overlay: a separate layer above the content inside the same window, which is the
    // QtQuick-native equivalent of an overlay surface (QtQuick.Controls.Popup is covered by the
    // capability-gated controls fixture).
    Rectangle {
        objectName: "overlay"
        id: overlay
        x: 250; y: 340; width: 200; height: 120
        color: "#eef2ff"
        border.color: "#88a"
        visible: false
        MouseArea {
            objectName: "overlayArea"
            anchors.fill: parent
            onClicked: bridge.inc("overlayClicks")
        }
    }

    MouseArea {
        objectName: "openOverlay"
        x: 0; y: 300; width: 120; height: 40
        onClicked: overlay.visible = true
    }
}
)QML",
                     QUrl(QStringLiteral("test_quick_input_preflight.qml")));
    QObject *qmlRoot = component.create();
    check(qmlRoot != nullptr, "qml: the inline QtQuick-only scene instantiates");
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

    const auto sceneCenter = [&window, qmlRootItem](const char *objectName) {
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

    // 1. Single click: Qt Quick owns item routing already.
    click(components, window, sceneCenter("area"));
    check(bridge.property("areaClicks").toInt() == 1,
          "MouseArea: single remote click reaches onClicked");
    noDivergence("Quick single click / item routing",
                 "Qt Quick resolves the item under the point itself; the adapter only delivers to the window");

    // 2. Valid double click: is onDoubleClicked reachable on this path?
    doubleClick(components, window, sceneCenter("area"));
    if (bridge.property("areaDoubleClicks").toInt() == 0) {
        divergence("QT_SEMANTIC_CLASSIFICATION",
                   "MouseArea onDoubleClicked",
                   "no DblClick event is ever delivered into the window, so MouseArea/SinglePointHandler "
                   "double-tap handlers are unreachable for remote users even though Qt Quick routing "
                   "itself is fine");
    } else {
        noDivergence("MouseArea onDoubleClicked", "a double-click semantic was observable");
    }

    // 3. Focusable text target.
    click(components, window, sceneCenter("textInput"));
    QQuickItem *inputItem = qmlRootItem->findChild<QQuickItem *>(QStringLiteral("textInput"));
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

    // 4. Drag: a held press plus moves must keep reaching the pressed item.
    const QPointF dragStart = sceneCenter("dragArea");
    postRaw(components, window, hyremote::InputEventKind::PointerMove, dragStart);
    postRaw(components, window, hyremote::InputEventKind::PointerButton, dragStart,
            hyremote::PointerButton::Left, true);
    pump();
    const int movesBefore = bridge.property("dragMoves").toInt();
    postRaw(components, window, hyremote::InputEventKind::PointerMove, dragStart + QPointF(60, 60));
    pump();
    postRaw(components, window, hyremote::InputEventKind::PointerButton, dragStart + QPointF(60, 60),
            hyremote::PointerButton::Left, false);
    pump();
    std::cout << "     observed[drag]: presses=" << bridge.property("dragPresses").toInt()
              << " moves=" << bridge.property("dragMoves").toInt() << " (before move: " << movesBefore
              << ")\n";
    check(bridge.property("dragPresses").toInt() >= 1, "MouseArea: the held press reaches the item");
    check(bridge.property("dragMoves").toInt() > movesBefore,
          "MouseArea: held moves keep reaching the pressed item");
    if (bridge.property("dragPresses").toInt() >= 1 && bridge.property("dragMoves").toInt() > movesBefore)
        noDivergence("Quick drag", "press/move/release reaches the dragged item");

    // 5. In-window overlay surface. Both steps are hard assertions: the overlay reachability claim
    //    is only printed when the remote path really opened it and really activated its content.
    click(components, window, sceneCenter("openOverlay"));
    QQuickItem *overlayItem = qmlRootItem->findChild<QQuickItem *>(QStringLiteral("overlay"));
    const bool overlayOpen = overlayItem && overlayItem->isVisible();
    check(overlayOpen, "overlay: the remote click opened the in-window overlay surface");
    if (overlayOpen) {
        click(components, window, sceneCenter("overlayArea"));
        const int overlayClicks = bridge.property("overlayClicks").toInt();
        std::cout << "     observed[overlay]: overlayClicks=" << overlayClicks << '\n';
        check(overlayClicks >= 1, "overlay: a remote click reaches the overlay content");
        if (overlayClicks >= 1)
            noDivergence("Quick in-window overlay input",
                         "an overlay layer inside the same window stays reachable on the window-delivery route");
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
