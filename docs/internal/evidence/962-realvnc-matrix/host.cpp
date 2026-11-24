#include <HyRemote/RemoteAccess.h>

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QHostAddress>
#include <QKeyEvent>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QTimer>
#include <windows.h>

#include <array>
#include <iostream>
#include <mutex>
#include <vector>

#ifdef HYREMOTE_RESEARCH_NATURAL_CREDIT
#include "native_quick_source.hpp"
#endif

class Marker final : public QQuickPaintedItem
{
public:
    using QQuickPaintedItem::QQuickPaintedItem;

    void setMarkerColor(QColor color)
    {
        m_color = color;
        update();
    }

    void paint(QPainter *painter) override
    {
        painter->fillRect(boundingRect(), m_color);
    }

private:
    QColor m_color;
};

// Research-only local input probe. WM_KEYDOWN is posted directly to the
// fixture HWND so it does not depend on, or change, foreground focus.
class ProbeWindow final : public QQuickWindow
{
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_F24) {
            LARGE_INTEGER timestamp;
            QueryPerformanceCounter(&timestamp);
            std::cout << "LOCAL_INPUT " << timestamp.QuadPart << std::endl;
            event->accept();
            return;
        }
        QQuickWindow::keyPressEvent(event);
    }
};

int main(int argc, char **argv)
{
#ifdef HYREMOTE_RESEARCH_NATURAL_CREDIT
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
#endif
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("port"), QStringLiteral("Loopback RFB port."));
    parser.process(app);
    bool valid = false;
    const auto args = parser.positionalArguments();
    const unsigned port = args.size() == 1 ? args.first().toUInt(&valid) : 0;
    if (!valid || port == 0 || port > 65535)
        return 2;

    const std::array colors{QColor("#c92a59"), QColor("#2bb77d"),
                            QColor("#374cdb"), QColor("#edb627")};
    ProbeWindow window;
    const bool matrixProbe = qEnvironmentVariableIsSet("HYREMOTE_RESEARCH_MATRIX");
    std::mutex swapsMutex;
    std::vector<LONGLONG> swaps;
    QMetaObject::Connection swapConnection;
    if (matrixProbe) {
        swapConnection = QObject::connect(&window, &QQuickWindow::frameSwapped, &window, [&] {
            LARGE_INTEGER timestamp;
            QueryPerformanceCounter(&timestamp);
            std::lock_guard<std::mutex> lock(swapsMutex);
            swaps.push_back(timestamp.QuadPart);
        }, Qt::DirectConnection);
    }
    window.setTitle(QStringLiteral("HyRemote latency fixture"));
    window.setGeometry(30, 70, 384, 240);
    Marker marker(window.contentItem());
    marker.setSize(QSizeF(window.size()));
    marker.setMarkerColor(colors.front());
    window.show();

#ifdef HYREMOTE_RESEARCH_NATURAL_CREDIT
    NativeQuickTarget target(window);
    HyRemote::RemoteAccess remote(&target);
#else
    HyRemote::RemoteAccess remote(&window);
#endif
    remote.setPort(static_cast<quint16>(port));
    if (!remote.setListenAddress(QHostAddress::LocalHost) || !remote.start()) {
        if (const auto error = remote.lastError())
            std::cerr << error->message.toStdString() << '\n';
        return 3;
    }
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    std::cout << "READY " << port << ' ' << frequency.QuadPart << std::endl;
    if (matrixProbe)
        std::cout << "HOST_HWND " << quintptr(window.winId()) << std::endl;
    QTimer timer;
    timer.setTimerType(Qt::PreciseTimer);
    int sequence = 0;
    QObject::connect(&timer, &QTimer::timeout, &app, [&] {
        ++sequence;
        const QColor color = colors[sequence % colors.size()];
        LARGE_INTEGER timestamp;
        QueryPerformanceCounter(&timestamp);
        marker.setMarkerColor(color);
        std::cout << "CHANGE " << sequence << ' ' << timestamp.QuadPart << ' '
                  << color.name().toStdString() << std::endl;
    });
    timer.start(1000);
    const int configuredDuration = qEnvironmentVariableIntValue("HYREMOTE_RESEARCH_HOST_MS");
    QTimer::singleShot(configuredDuration > 0 ? configuredDuration : 45000,
                       Qt::PreciseTimer, &app, &QCoreApplication::quit);
    const int exit = app.exec();
    remote.stop();
    if (matrixProbe) {
        QObject::disconnect(swapConnection);
        window.hide();
        window.destroy();
        const std::lock_guard<std::mutex> lock(swapsMutex);
        for (const auto tick : swaps)
            std::cout << "FRAME_SWAPPED " << tick << '\n';
        std::cout << "STOPPED " << int(remote.state() == HyRemote::RemoteAccessState::Stopped)
                  << std::endl;
    }
    return exit;
}
