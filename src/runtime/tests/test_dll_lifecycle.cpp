#include "../../integrations/qpa/tests/rfb_test_client.hpp"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QTcpServer>
#include <QTimer>

#include <windows.h>
#include <iostream>

namespace {
using HyRemote::Qpa::Test::RfbTestClient;
using HyRemote::Qpa::Test::pumpEvents;

bool runCycle(const wchar_t *dllPath, QQuickWindow &window, const QColor &expected)
{
    if (GetModuleHandleW(L"HyRemoteRemoteAccess.dll"))
        return false;
    const HMODULE runtime = LoadLibraryExW(dllPath, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!runtime)
        return false;

    const auto start = reinterpret_cast<void *(*)(QObject *, quint16)>(
        GetProcAddress(runtime, "hyremote_test_start"));
    const auto stop = reinterpret_cast<bool (*)(void *)>(
        GetProcAddress(runtime, "hyremote_test_stop"));
    const auto resources = reinterpret_cast<int (*)()>(
        GetProcAddress(runtime, "hyremote_test_gl_resources"));
    if (!start || !stop || !resources || resources() != 0)
        return false;

    QTcpServer reservation;
    if (!reservation.listen(QHostAddress::LocalHost))
        return false;
    const quint16 port = reservation.serverPort();
    reservation.close();
    window.setColor(expected);
    window.update();
    void *instance = start(&window, port);
    if (!instance)
        return false;

    QTimer frames;
    QObject::connect(&frames, &QTimer::timeout, &window, [&window] { window.update(); });
    frames.start(16);
    RfbTestClient viewer(port);
    bool sampled = false;
    if (viewer.connectAndHandshake()) {
        for (int i = 0; i < 6 && viewer.requestFramebuffer(1500); ++i) {
            const auto &image = viewer.image();
            if (!image.isNull() && image.pixelColor(image.rect().center()) == expected
                && resources() == 1) {
                sampled = true;
                break;
            }
        }
    }
    frames.stop();
    const bool stopped = stop(instance);
    const bool drained = resources() == 2;
    // Deliberately unload before spinning the GUI queue. If Runtime queued
    // work survived stop, dispatch and another real render must expose it.
    const bool unloaded = FreeLibrary(runtime) && !GetModuleHandleW(L"HyRemoteRemoteAccess.dll");
    window.update();
    pumpEvents(250);
    std::cerr << "SDK_DLL sampled=" << sampled << " stopped=" << stopped
              << " resources_drained=" << drained << " module_unmapped=" << unloaded
              << " viewer_disconnected=" << !viewer.connected() << '\n';
    return sampled && stopped && drained && unloaded && !viewer.connected();
}
} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 2)
        return 1;
    const auto path = QString::fromLocal8Bit(argv[1]).toStdWString();
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    {
        QQuickWindow window;
        window.resize(320, 200);
        window.show();
        pumpEvents(250);
        if (!runCycle(path.c_str(), window, QColor(215, 36, 57))
            || !runCycle(path.c_str(), window, QColor(37, 212, 66)))
            return 2;
    }
    pumpEvents(100);
    std::cerr << "SDK_DLL_TWO_CYCLES=PASS" << std::endl;
    return 0;
}
