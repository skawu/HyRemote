#include <QColor>
#include <QCoreApplication>
#include <QHostAddress>
#include <QQmlEngine>
#include <QWidget>
#include <QtGlobal>
#include <QtQuick/QQuickWindow>

#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QHostAddress wildcard(QHostAddress::AnyIPv4);
    const QColor color(Qt::red);
    if (wildcard.isNull() || !color.isValid())
        return 2;

    // Reference one exported symbol/data object from every public component the preflight claims:
    // Core (qVersion/QCoreApplication), Network (QHostAddress), Gui (QColor), Widgets,
    // Quick and Qml (their exported metaobjects). No GUI event loop/window is required.
    const char *widgetClass = QWidget::staticMetaObject.className();
    const char *quickClass = QQuickWindow::staticMetaObject.className();
    const char *qmlClass = QQmlEngine::staticMetaObject.className();
    if (!widgetClass || !quickClass || !qmlClass)
        return 3;

    std::cout << "RUNTIME_QT_VERSION=" << qVersion() << '\n';
    std::cout << "LINKED_WIDGETS_CLASS=" << widgetClass << '\n';
    std::cout << "LINKED_QUICK_CLASS=" << quickClass << '\n';
    std::cout << "LINKED_QML_CLASS=" << qmlClass << '\n';
    std::cout << "PUBLIC_LINK_PROBE=PASS\n";
    return 0;
}
