// SPDX-License-Identifier: Apache-2.0
#include <QGuiApplication>
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

    bool eventLoopTicked = false;
    QTimer::singleShot(0, &app, [&eventLoopTicked] { eventLoopTicked = true; });

    QTimer::singleShot(250, &app, [&]() {
        if (!eventLoopTicked) {
            std::fprintf(stderr, "FAIL: proxy event loop did not dispatch a queued timer\n");
            app.exit(4);
            return;
        }
        if (!window.handle()) {
            std::fprintf(stderr, "FAIL: native delegate platform window disappeared during smoke run\n");
            app.exit(5);
            return;
        }
        std::printf("PASS: HyRemote QPA proxy loaded, native delegate created a platform window, event loop is alive\n");
        window.close();
        app.quit();
    });

    return app.exec();
}
