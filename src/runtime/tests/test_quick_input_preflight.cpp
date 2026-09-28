// Behavioral acceptance for the Qt Quick remote pointer path after ingress moved to
// QWindowSystemInterface. The Runtime supplies raw pointer facts; Qt Quick owns item routing,
// grabs and click classification.

#include <QCoreApplication>
#include <QEventLoop>
#include <QGuiApplication>
#include <QObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStyleHints>
#include <QThread>

#include <iostream>

#include "detail/component_factories.hpp"
#include "hyremote/core/input.hpp"

namespace {

int failures = 0;

void check(bool ok, const char *what)
{
    if (!ok) {
        std::cerr << "CHECK failed: " << what << '\n';
        ++failures;
    }
    std::cout << (ok ? "ok   " : "FAIL ") << what << '\n';
}

void pump()
{
    for (int i = 0; i < 20; ++i)
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

void postPointer(HyRemote::detail::TargetComponents &components,
                 const QQuickWindow &window,
                 hyremote::InputEventKind kind,
                 const QPointF &point,
                 hyremote::PointerButton button = hyremote::PointerButton::None,
                 bool pressed = true)
{
    hyremote::InputEvent event;
    event.kind = kind;
    event.sourceViewport = {static_cast<std::uint32_t>(window.width()),
                            static_cast<std::uint32_t>(window.height()),
                            1.0F};
    event.x = static_cast<float>(point.x());
    event.y = static_cast<float>(point.y());
    event.button = button;
    event.pressed = pressed;
    components.input->post(event);
}

void click(HyRemote::detail::TargetComponents &components,
           const QQuickWindow &window,
           const QPointF &point)
{
    postPointer(components, window, hyremote::InputEventKind::PointerButton, point,
                hyremote::PointerButton::Left, true);
    postPointer(components, window, hyremote::InputEventKind::PointerButton, point,
                hyremote::PointerButton::Left, false);
    pump();
}

void doubleClick(HyRemote::detail::TargetComponents &components,
                 const QQuickWindow &window,
                 const QPointF &point)
{
    postPointer(components, window, hyremote::InputEventKind::PointerButton, point,
                hyremote::PointerButton::Left, true);
    postPointer(components, window, hyremote::InputEventKind::PointerButton, point,
                hyremote::PointerButton::Left, false);
    pump();
    QThread::msleep(5);
    postPointer(components, window, hyremote::InputEventKind::PointerButton, point,
                hyremote::PointerButton::Left, true);
    postPointer(components, window, hyremote::InputEventKind::PointerButton, point,
                hyremote::PointerButton::Left, false);
    pump();
}

void runAcceptance()
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

    MouseArea {
        objectName: "area"
        x: 0; y: 0; width: 200; height: 200
        onClicked: bridge.inc("clicks")
        onDoubleClicked: bridge.inc("doubleClicks")
    }

    MouseArea {
        objectName: "dragArea"
        x: 250; y: 20; width: 100; height: 100
        onPressed: bridge.inc("dragPresses")
        onPositionChanged: if (pressed) bridge.inc("dragMoves")
    }

    Rectangle {
        objectName: "overlay"
        id: overlay
        x: 250; y: 250; width: 180; height: 120
        visible: false
        MouseArea {
            objectName: "overlayArea"
            anchors.fill: parent
            onClicked: bridge.inc("overlayClicks")
        }
    }

    MouseArea {
        objectName: "openOverlay"
        x: 0; y: 250; width: 140; height: 60
        onClicked: overlay.visible = true
    }
}
)QML",
                      QUrl(QStringLiteral("test_quick_input_preflight.qml")));

    QObject *object = component.create();
    check(object != nullptr, "inline Qt Quick scene instantiates");
    if (!object) {
        for (const QQmlError &error : component.errors())
            std::cerr << error.toString().toStdString() << '\n';
        return;
    }

    auto *rootItem = qobject_cast<QQuickItem *>(object);
    check(rootItem != nullptr, "inline scene root is a QQuickItem");
    if (!rootItem) {
        delete object;
        return;
    }

    QQuickWindow window;
    window.resize(500, 500);
    rootItem->setParentItem(window.contentItem());
    window.show();
    pump();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, true);
    check(components.input != nullptr, "quick input component is available");
    if (!components.input) {
        delete object;
        return;
    }

    const auto center = [rootItem](const char *name) {
        QQuickItem *item = rootItem->findChild<QQuickItem *>(QString::fromLatin1(name));
        if (!item)
            return QPointF(-1, -1);
        return item->mapToScene(QPointF(item->width() / 2.0, item->height() / 2.0));
    };

    click(components, window, center("area"));
    check(bridge.property("clicks").toInt() == 1,
          "Qt Quick owns MouseArea hit testing for a remote click");

    QThread::msleep(static_cast<unsigned long>(QGuiApplication::styleHints()->mouseDoubleClickInterval())
                    + 20UL);
    bridge.setProperty("doubleClicks", 0);
    doubleClick(components, window, center("area"));
    check(bridge.property("doubleClicks").toInt() == 1,
          "Qt classifies the remote sequence and Qt Quick delivers onDoubleClicked");

    QThread::msleep(static_cast<unsigned long>(QGuiApplication::styleHints()->mouseDoubleClickInterval())
                    + 20UL);
    bridge.setProperty("doubleClicks", 0);
    click(components, window, center("area"));
    postPointer(components, window, hyremote::InputEventKind::PointerMove, QPointF(480, 480));
    postPointer(components, window, hyremote::InputEventKind::PointerMove, center("area"));
    pump();
    QThread::msleep(5);
    click(components, window, center("area"));
    check(bridge.property("doubleClicks").toInt() == 0,
          "Qt itself invalidates a double click after a far pointer excursion");

    const QPointF dragStart = center("dragArea");
    postPointer(components, window, hyremote::InputEventKind::PointerButton, dragStart,
                hyremote::PointerButton::Left, true);
    pump();
    postPointer(components, window, hyremote::InputEventKind::PointerMove, dragStart + QPointF(140, 140));
    pump();
    postPointer(components, window, hyremote::InputEventKind::PointerButton,
                dragStart + QPointF(140, 140), hyremote::PointerButton::Left, false);
    pump();
    check(bridge.property("dragPresses").toInt() == 1,
          "Qt Quick receives the remote press");
    check(bridge.property("dragMoves").toInt() >= 1,
          "Qt Quick owns the implicit grab during the remote drag");

    click(components, window, center("openOverlay"));
    QQuickItem *overlay = rootItem->findChild<QQuickItem *>(QStringLiteral("overlay"));
    check(overlay && overlay->isVisible(), "in-window overlay opens normally");
    click(components, window, center("overlayArea"));
    check(bridge.property("overlayClicks").toInt() == 1,
          "Qt Quick routes remote input to an in-window overlay without HyRemote item logic");

    components.input.reset();
    delete object;
}

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    runAcceptance();
    HyRemote::detail::resetFactories();

    std::cout << (failures == 0 ? "PASS: quick window-system input acceptance"
                                : "FAIL: quick window-system input acceptance")
              << " (" << failures << " checks failed)\n";
    return failures == 0 ? 0 : 1;
}

#include "test_quick_input_preflight.moc"
