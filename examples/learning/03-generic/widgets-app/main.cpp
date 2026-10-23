// HyRemote V0.1 learning example 03, Widgets application.
//
// This is an ORDINARY Qt Widgets application. It has:
//
//   * zero HyRemote headers;
//   * zero HyRemote API calls;
//   * zero HyRemote application link libraries.
//
// That is the point: the same Qt-only executable runs normally, or reaches HyRemote with no source change at all
// when it is launched with the Generic plugin:
//
//     ./hyremote-learning-03-widgets                       # ordinary launch, no HyRemote
//     ./hyremote-learning-03-widgets -plugin hyremote      # same binary, HyRemote becomes reachable
//
// The only HyRemote-related thing in this directory is in CMakeLists.txt, and it is packaging, not application
// code: the deployment helper copies the Generic payload, the shared runtime and the Qt runtime closure into the
// deployed tree.
//
// The application prints two lines the smoke test reads: `APP_READY`, and the native Qt platform it is really
// running on. Under Generic activation the platform must stay the same - the Generic Plugin preserves the
// application's native platform integration instead of replacing it.

#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <iostream>

namespace {

// A deliberately small hand-rolled parser: an ordinary Qt application should not fight with Qt's own platform
// arguments such as `-plugin hyremote:port=...`, which are consumed before main() body arguments reach the app.
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
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HyRemote Learning 03 - Widgets (Qt only)"));

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("Learning 03 - ordinary Qt Widgets application"));
    window.resize(460, 190);

    auto *label = new QLabel(&window);
    label->setWordWrap(true);
    label->setText(QStringLiteral("This is an ordinary Qt Widgets application.\n\n"
                                  "It contains no HyRemote code and links no HyRemote library.\n"
                                  "Launch it with  -plugin hyremote  to make it remotely viewable "
                                  "with no source change."));
    window.setCentralWidget(label);
    window.show();

    std::cout << "APP_READY" << std::endl;
    std::cout << "PLATFORM_NAME=" << QGuiApplication::platformName().toStdString() << std::endl;

    const int testSeconds = testSecondsFrom(QCoreApplication::arguments());
    if (testSeconds > 0)
        QTimer::singleShot(testSeconds * 1000, &app, &QCoreApplication::quit);

    return app.exec();
}
