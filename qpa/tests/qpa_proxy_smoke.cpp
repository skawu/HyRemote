#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QTimer>
#include <QWindow>

#include <cstdio>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    if (QGuiApplication::platformName().compare(QStringLiteral("hyremote"), Qt::CaseInsensitive) != 0) {
        std::fprintf(stderr,
                     "FAIL: expected HyRemote QPA platform, got '%s'\n",
                     qPrintable(QGuiApplication::platformName()));
        return 2;
    }

    QWindow window;
    window.setTitle(QStringLiteral("HyRemote QPA Proxy Smoke"));
    window.resize(320, 180);
    window.show();

    if (!window.handle()) {
        std::fprintf(stderr, "FAIL: native delegate did not create a platform window\n");
        return 3;
    }

    bool exposedOrCreated = window.handle() != nullptr;
    QObject::connect(&window, &QWindow::visibleChanged, &app, [&exposedOrCreated](bool visible) {
        if (visible)
            exposedOrCreated = true;
    });

    QTimer::singleShot(250, &app, [&]() {
        if (!exposedOrCreated) {
            std::fprintf(stderr, "FAIL: proxy window never became locally visible/created\n");
            app.exit(4);
            return;
        }
        std::printf("PASS: HyRemote QPA proxy loaded, native delegate created a local window, event loop is alive\n");
        window.close();
        app.quit();
    });

    return app.exec();
}
