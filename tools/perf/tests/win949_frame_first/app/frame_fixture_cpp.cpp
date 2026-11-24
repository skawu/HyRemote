// WIN949 NON-PRODUCT Qt Quick frame fixture, C++ API route (#949).
//
// Why this variant exists: the Generic plugin is Qt6-only (it hard-requires
// Qt6::Gui), so the Qt 5.15.2 LTS comparison in #949 has to use a legal C++/Quick
// integration instead of the -plugin route. Same scene, same WIN949_MARK /
// WIN949_POINTER timestamp contract, same CLI as frame_fixture.cpp, plus an
// explicit HyRemote::RemoteAccess listener.
//
// This file lives in the non-product test tooling tree only.

#include "fixture_scene.hpp"

#include <HyRemote/RemoteAccess.h>

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QHostAddress>
#include <QQuickWindow>
#include <QTimer>

using namespace win949;

namespace {

void emitReport(HyRemote::RemoteAccess &access, const char *tag)
{
    emitLine(tag, QStringLiteral("state=%1 connected=%2")
                       .arg(static_cast<int>(access.state()))
                       .arg(static_cast<quint64>(access.connectedClientCount())));
    const QString report = access.diagnosticReport();
    for (const QString &line : report.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
        emitLine("WIN949_API_REPORT", line);
}

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HyRemote WIN949 frame fixture (C++ API)"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Non-product #949 Windows frame fixture (C++ API route)"));
    parser.addHelpOption();
    const QCommandLineOption portOption(QStringLiteral("port"), QStringLiteral("listener port"), QStringLiteral("port"),
                                        QStringLiteral("5921"));
    const QCommandLineOption inputOption(QStringLiteral("remote-input"),
                                         QStringLiteral("enable remote keyboard/pointer input"));
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
    parser.addOptions({portOption, inputOption, sizeOption, scenarioOption, phaseOption, secondsOption, hideOption,
                       showOption, resizeOption, resizeToOption});
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
    const int port = parser.value(portOption).toInt();
    const bool remoteInput = parser.isSet(inputOption);

    QQuickWindow window;
    window.setFlags(Qt::FramelessWindowHint);
    window.setColor(Qt::black);
    window.setTitle(QStringLiteral("WIN949 frame fixture (C++ API)"));
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

    HyRemote::RemoteAccess access(&window);
    // The product's listener contract is IPv4-only (`listen address must be an
    // IPv4 address`), so AnyIPv4 is the supported zero-configuration binding.
    bool started = access.setListenAddress(QHostAddress::AnyIPv4);
    started = started && access.setPort(static_cast<quint16>(port));
    started = started && access.setRemoteInputEnabled(remoteInput);
    started = started && access.start();
    if (!started) {
        emitReport(access, "WIN949_API_START");
        emitLine("WIN949_API_FAILED", QStringLiteral("setPort/start rejected"));
        return 3;
    }
    emitReport(access, "WIN949_API_START");

    // Client-count changes are the only state this fixture reports on, so the
    // 0/1/2-Viewer scenarios keep one authoritative line per transition.
    quint64 reportedClients = access.connectedClientCount();
    QTimer reportTimer;
    QObject::connect(&reportTimer, &QTimer::timeout, &app, [&access, &reportedClients] {
        const quint64 current = access.connectedClientCount();
        if (current == reportedClients)
            return;
        reportedClients = current;
        emitReport(access, "WIN949_API_CLIENTS");
    });
    reportTimer.start(200);

    emitLine("WIN949_READY", QStringLiteral("scenario=%1 size=%2x%3 dpr=%4 platform=%5 route=cpp-api")
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
                 QStringLiteral("seq=%1 state=%2 content_change=1 %3")
                     .arg(*markerSeq).arg(phase).arg(stamp()));
    };

    QTimer markerTimer;
    QTimer moverTimer;
    if (scenario == QLatin1String("local") || scenario == QLatin1String("lifecycle")) {
        // #952: prove a real static-idle -> Qt content-change transition. During
        // this initial window nothing asks QQuickWindow to repaint, so an
        // afterAnimating hint there would be grab-induced, not real content.
        const int initialIdleMs = 3000;
        emitLine("WIN949_IDLE_BEGIN",
                 QStringLiteral("static_ms=%1 %2").arg(initialIdleMs).arg(stamp()));
        QObject::connect(&markerTimer, &QTimer::timeout, scene, toggleMarker);
        QTimer::singleShot(initialIdleMs, &app, [&markerTimer, phaseMs, toggleMarker] {
            toggleMarker();
            emitLine("WIN949_IDLE_END", QStringLiteral("first_content_change=1 %1").arg(stamp()));
            markerTimer.start(phaseMs);
        });
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
