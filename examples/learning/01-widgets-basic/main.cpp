#include <HyRemote/RemoteAccess.h>

#include <QApplication>
#include <QCoreApplication>
#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("HyRemote Widgets Basic"));

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("HyRemote - Widgets Basic"));
    window.resize(720, 420);

    auto *message = new QLabel(
        QStringLiteral("This is a normal Qt Widgets window.\n\n"
                       "HyRemote attaches through the C++ API and starts only when start() is called.\n"
                       "The default listener is loopback-only and remote input is disabled."),
        &window);
    message->setAlignment(Qt::AlignCenter);
    message->setWordWrap(true);
    window.setCentralWidget(message);

    HyRemote::RemoteAccess remote(&window);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&remote] {
        remote.stop();
    });

    window.show();

    if (remote.start()) {
        window.statusBar()->showMessage(
            QStringLiteral("Remote view ready at %1:%2 (view-only default)")
                .arg(remote.listenAddress().toString())
                .arg(remote.port()));
    } else {
        const auto error = remote.lastError();
        window.statusBar()->showMessage(
            error ? QStringLiteral("HyRemote start failed: %1").arg(error->message)
                  : QStringLiteral("HyRemote start failed"));
    }

    return app.exec();
}
