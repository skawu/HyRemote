// HyRemote V0.1 learning example 03, Quick application.
//
// The same ordinary-Qt-only story as the Widgets application in this directory: zero HyRemote headers, zero HyRemote
// API calls, zero HyRemote application link libraries. The QML file is plain Qt Quick.
//
//     ./hyremote-learning-03-quick                       # ordinary launch, no HyRemote
//     ./hyremote-learning-03-quick -plugin hyremote      # same binary, HyRemote becomes reachable
//
// The application prints `APP_READY` and the native Qt platform it is really running on; under Generic activation
// that platform must be unchanged.

#include <QGuiApplication>
#include <QQmlError>
#include <QQuickView>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include <iostream>

namespace {

int testSecondsFrom(const QStringList &arguments)
{
    const int index = arguments.indexOf(QStringLiteral("--test-seconds"));
    if (index < 0 || index + 1 >= arguments.size())
        return 0;
    bool ok = false;
    const int seconds = arguments.at(index + 1).toInt(&ok);
    return ok && seconds > 0 ? seconds : 0;
}

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HyRemote Learning 03 - Quick (Qt only)"));

    QQuickView view;
    view.setTitle(QStringLiteral("Learning 03 - ordinary Qt Quick application"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(460, 190);
    view.setSource(QUrl(QStringLiteral("qrc:/hyremote/learning/03-zero-code-generic/Main.qml")));
    if (view.status() == QQuickView::Error) {
        for (const QQmlError &error : view.errors())
            std::cerr << error.toString().toStdString() << std::endl;
        return 65;
    }
    view.show();

    std::cout << "APP_READY" << std::endl;
    std::cout << "PLATFORM_NAME=" << QGuiApplication::platformName().toStdString() << std::endl;

    const int testSeconds = testSecondsFrom(QCoreApplication::arguments());
    if (testSeconds > 0)
        QTimer::singleShot(testSeconds * 1000, &app, &QCoreApplication::quit);

    return app.exec();
}
