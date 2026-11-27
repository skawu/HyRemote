// WIN949 NON-PRODUCT Qt Quick frame fixture, Generic-plugin route (#949).
//
// This is NOT product code and is never built into the SDK/install tree. It is
// an ordinary Qt Quick application (zero HyRemote headers, zero HyRemote API
// calls) that is launched with the product's Generic plugin:
//
//     frame_fixture.exe -plugin hyremote:port=<port>
//
// The scene lives in fixture_scene.hpp and is shared with the C++-API variant
// (frame_fixture_cpp.cpp), which is the only route Qt 5.15 can legally use
// because the Generic plugin is Qt6-only.
//
// Modes:
//   --scenario=static   paint once, never change again (quasi-static baseline)
//   --scenario=local    toggle the marker rectangle only (localized damage)
//   --scenario=motion   move a filled square every tick and toggle the marker
//   --scenario=input    draw a pointer indicator at the last remote pointer event
//
// Scene geometry is logical pixels; the framebuffer a Viewer sees is
// logical * devicePixelRatio.

#include "fixture_scene.hpp"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>

using namespace win949;

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HyRemote WIN949 frame fixture"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Non-product #949 Windows frame fixture"));
    parser.addHelpOption();
    const QCommandLineOption sizeOption(QStringLiteral("size"), QStringLiteral("WxH"), QStringLiteral("geometry"),
                                        QStringLiteral("1920x1080"));
    const QCommandLineOption scenarioOption(QStringLiteral("scenario"),
                                            QStringLiteral("static|local|motion|input"), QStringLiteral("mode"),
                                            QStringLiteral("local"));
    const QCommandLineOption phaseOption(QStringLiteral("phase-ms"), QStringLiteral("marker cadence"),
                                         QStringLiteral("ms"), QStringLiteral("250"));
    const QCommandLineOption secondsOption(QStringLiteral("test-seconds"), QStringLiteral("self exit"),
                                           QStringLiteral("seconds"), QStringLiteral("0"));
    const QCommandLineOption hideOption(QStringLiteral("hide-at"), QStringLiteral("hide window at ms"),
                                        QStringLiteral("ms"), QStringLiteral("0"));
    const QCommandLineOption showOption(QStringLiteral("show-at"), QStringLiteral("show window at ms"),
                                        QStringLiteral("ms"), QStringLiteral("0"));
    const QCommandLineOption resizeOption(QStringLiteral("resize-at"), QStringLiteral("resize at ms"),
                                          QStringLiteral("ms"), QStringLiteral("0"));
    const QCommandLineOption resizeToOption(QStringLiteral("resize-to"), QStringLiteral("WxH"),
                                            QStringLiteral("geometry"), QStringLiteral("1280x720"));
    parser.addOptions({sizeOption, scenarioOption, phaseOption, secondsOption, hideOption, showOption,
                       resizeOption, resizeToOption});
    parser.process(app);

    const QStringList sizeParts = parser.value(sizeOption).split(QLatin1Char('x'));
    int width = sizeParts.size() == 2 ? sizeParts.at(0).toInt() : 1920;
    int height = sizeParts.size() == 2 ? sizeParts.at(1).toInt() : 1080;
    if (width <= 0 || height <= 0) {
        width = 1920;
        height = 1080;
    }
    const QString scenario = parser.value(scenarioOption);
    const int phaseMs = qMax(10, parser.value(phaseOption).toInt());
    const int testSeconds = parser.value(secondsOption).toInt();

    QQuickWindow window;
    window.setFlags(Qt::FramelessWindowHint);
    window.setColor(Qt::black);
    window.setTitle(QStringLiteral("WIN949 frame fixture"));
    window.resize(width, height);
    window.setPosition(0, 0);

    auto *scene = new FixtureScene;
    scene->setParentItem(window.contentItem());
    scene->setSceneGeometry(width, height);
    QObject::connect(&window, &QQuickWindow::widthChanged, scene, [&] {
        scene->setSceneGeometry(static_cast<int>(window.width()), static_cast<int>(window.height()));
    });
    QObject::connect(&window, &QQuickWindow::heightChanged, scene, [&] {
        scene->setSceneGeometry(static_cast<int>(window.width()), static_cast<int>(window.height()));
    });
    window.show();

    emitLine("WIN949_READY", QStringLiteral("scenario=%1 size=%2x%3 dpr=%4 platform=%5")
                                 .arg(scenario)
                                 .arg(width)
                                 .arg(height)
                                 .arg(window.devicePixelRatio())
                                 .arg(QGuiApplication::platformName()));

    int *markerSeq = new int(0);
    auto toggleMarker = [scene, markerSeq]() {
        const int phase = ((*markerSeq + 1) % kMarkerPhases);
        ++*markerSeq;
        scene->setMarkerPhase(phase);
        emitLine("WIN949_MARK",
                 QStringLiteral("seq=%1 state=%2 %3").arg(*markerSeq).arg(phase).arg(stamp()));
    };

    QTimer markerTimer;
    QTimer moverTimer;
    if (scenario == QLatin1String("local") || scenario == QLatin1String("lifecycle")) {
        QObject::connect(&markerTimer, &QTimer::timeout, scene, toggleMarker);
        markerTimer.start(phaseMs);
    } else if (scenario == QLatin1String("motion")) {
        QObject::connect(&markerTimer, &QTimer::timeout, scene, toggleMarker);
        markerTimer.start(phaseMs);
        QObject::connect(&moverTimer, &QTimer::timeout, scene, [scene, width, height] {
            static int x = kMoverSize;
            static int dir = 1;
            x += dir * 8;
            if (x <= 0 || x + kMoverSize >= width) {
                dir = -dir;
                x = qBound(0, x, qMax(0, width - kMoverSize));
            }
            scene->setMoverPosition(x, (height - kMoverSize) / 2);
        });
        moverTimer.start(16);
    }

    const int hideAt = parser.value(hideOption).toInt();
    const int showAt = parser.value(showOption).toInt();
    const int resizeAt = parser.value(resizeOption).toInt();
    if (hideAt > 0) {
        QTimer::singleShot(hideAt, &window, [&window] {
            window.hide();
            emitLine("WIN949_HIDE", stamp());
        });
    }
    if (showAt > 0) {
        QTimer::singleShot(showAt, &window, [&window] {
            window.show();
            emitLine("WIN949_SHOW", stamp());
        });
    }
    if (resizeAt > 0) {
        QTimer::singleShot(resizeAt, &window, [&window, scene, &parser, &resizeToOption] {
            const QStringList parts = parser.value(resizeToOption).split(QLatin1Char('x'));
            const int rw = parts.size() == 2 ? parts.at(0).toInt() : 1280;
            const int rh = parts.size() == 2 ? parts.at(1).toInt() : 720;
            window.resize(rw, rh);
            scene->setSceneGeometry(rw, rh);
            emitLine("WIN949_RESIZE", QStringLiteral("w=%1 h=%2 %3").arg(rw).arg(rh).arg(stamp()));
        });
    }

    if (testSeconds > 0)
        QTimer::singleShot(testSeconds * 1000, &app, &QCoreApplication::quit);

    return app.exec();
}
