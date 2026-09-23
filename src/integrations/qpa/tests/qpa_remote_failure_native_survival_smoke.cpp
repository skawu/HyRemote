#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#elif defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#error "HyRemote QPA V1 native-survival smoke supports only Windows/Linux"
#endif

#include <QApplication>
#include <QByteArray>
#include <QMessageLogContext>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#include <atomic>
#include <cstdio>

namespace {

std::atomic_bool gRemoteStartFailureSeen{false};

void captureRemoteFailure(QtMsgType, const QMessageLogContext &, const QString &message)
{
    // Automatic application access is runtime-owned and shared by Generic + QPA. The QPA smoke
    // therefore observes the frontend-neutral runtime diagnostic rather than a historical QPA-only
    // controller string. This still proves that the QPA bootstrap attempted remote startup and that
    // the native delegate survived the forced listener failure.
    if (message.contains(
            QStringLiteral("HyRemote automatic access could not start the composite runtime"))) {
        gRemoteStartFailureSeen.store(true, std::memory_order_relaxed);
    }

    const QByteArray encoded = message.toLocal8Bit();
    std::fprintf(stderr, "%s\n", encoded.constData());
}

class PortReservation final
{
public:
    PortReservation() = default;
    PortReservation(const PortReservation &) = delete;
    PortReservation &operator=(const PortReservation &) = delete;

    ~PortReservation()
    {
#if defined(_WIN32)
        if (m_socket != INVALID_SOCKET)
            closesocket(m_socket);
        if (m_winsockStarted)
            WSACleanup();
#elif defined(__linux__)
        if (m_socket >= 0)
            ::close(m_socket);
#endif
    }

    bool reserve()
    {
#if defined(_WIN32)
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
            return false;
        m_winsockStarted = true;

        m_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET)
            return false;

        BOOL exclusive = TRUE;
        if (::setsockopt(m_socket,
                         SOL_SOCKET,
                         SO_EXCLUSIVEADDRUSE,
                         reinterpret_cast<const char *>(&exclusive),
                         static_cast<int>(sizeof(exclusive))) == SOCKET_ERROR) {
            return false;
        }
#else
        m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket < 0)
            return false;
#endif

        sockaddr_in address{};
        address.sin_family = AF_INET;
        // #174: the listener defaults to the wildcard, so the blocker must occupy the same address it will ask for.
    // A loopback blocker would only collide with a loopback listener, and relying on how a wildcard and a
    // loopback bind happen to overlap is exactly the platform detail this smoke must not depend on.
    address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(0);

#if defined(_WIN32)
        if (::bind(m_socket,
                   reinterpret_cast<const sockaddr *>(&address),
                   static_cast<int>(sizeof(address))) == SOCKET_ERROR) {
            return false;
        }
        if (::listen(m_socket, 1) == SOCKET_ERROR)
            return false;

        int addressLength = static_cast<int>(sizeof(address));
        if (::getsockname(m_socket,
                          reinterpret_cast<sockaddr *>(&address),
                          &addressLength) == SOCKET_ERROR) {
            return false;
        }
#else
        if (::bind(m_socket,
                   reinterpret_cast<const sockaddr *>(&address),
                   sizeof(address)) != 0) {
            return false;
        }
        if (::listen(m_socket, 1) != 0)
            return false;

        socklen_t addressLength = sizeof(address);
        if (::getsockname(m_socket,
                          reinterpret_cast<sockaddr *>(&address),
                          &addressLength) != 0) {
            return false;
        }
#endif

        m_port = ntohs(address.sin_port);
        return m_port != 0;
    }

    quint16 port() const noexcept { return m_port; }

    bool active() const noexcept
    {
#if defined(_WIN32)
        return m_socket != INVALID_SOCKET && m_port != 0;
#else
        return m_socket >= 0 && m_port != 0;
#endif
    }

private:
#if defined(_WIN32)
    SOCKET m_socket = INVALID_SOCKET;
    bool m_winsockStarted = false;
#else
    int m_socket = -1;
#endif
    quint16 m_port = 0;
};

}  // namespace

int main(int argc, char **argv)
{
    PortReservation blocker;
    if (!blocker.reserve()) {
        std::fprintf(stderr, "FAIL: could not reserve loopback port for QPA remote-failure test\n");
        return 1;
    }

    const quint16 blockedPort = blocker.port();
    const QByteArray platform = QByteArrayLiteral("hyremote:hyremote-port=")
                                + QByteArray::number(blockedPort);
    qputenv("QT_QPA_PLATFORM", platform);

    // Capturing the shared automatic-access diagnostic prevents a false pass where the native app is
    // healthy only because the QPA bootstrap never attempted to start the remote runtime.
    const QtMessageHandler previousHandler = qInstallMessageHandler(captureRemoteFailure);

    QApplication app(argc, argv);
    if (QApplication::platformName().compare(QStringLiteral("hyremote"), Qt::CaseInsensitive) != 0) {
        std::fprintf(stderr, "FAIL: HyRemote QPA plugin was not selected\n");
        qInstallMessageHandler(previousHandler);
        return 2;
    }

    QWidget window;
    window.setWindowTitle(QStringLiteral("HyRemote QPA remote-failure native survival"));
    window.resize(320, 180);
    window.show();

    if (!window.windowHandle() || !window.windowHandle()->handle()) {
        std::fprintf(stderr,
                     "FAIL: native delegate did not create a platform window while remote port was occupied\n");
        qInstallMessageHandler(previousHandler);
        return 3;
    }

    bool eventLoopTicked = false;
    QTimer::singleShot(0, &app, [&eventLoopTicked] { eventLoopTicked = true; });

    int result = 1;
    QTimer::singleShot(300, &app, [&] {
        if (!eventLoopTicked) {
            std::fprintf(stderr, "FAIL: native event loop stopped after remote listener failure\n");
            app.quit();
            return;
        }
        if (!gRemoteStartFailureSeen.load(std::memory_order_relaxed)) {
            std::fprintf(stderr,
                         "FAIL: shared automatic-access runtime did not report the forced listener startup failure\n");
            app.quit();
            return;
        }
        if (!blocker.active() || blocker.port() != blockedPort) {
            std::fprintf(stderr, "FAIL: occupied port unexpectedly changed ownership\n");
            app.quit();
            return;
        }
        if (!window.isVisible() || !window.windowHandle() || !window.windowHandle()->handle()) {
            std::fprintf(stderr, "FAIL: native application surface did not survive remote bind failure\n");
            app.quit();
            return;
        }

        std::printf("PASS: forced remote bind failure was observed and the native QPA application remained live\n");
        result = 0;
        window.close();
        app.quit();
    });

    QTimer::singleShot(5000, &app, [&] {
        std::fprintf(stderr, "FAIL: QPA remote-failure native survival smoke timed out\n");
        app.quit();
    });

    app.exec();
    qInstallMessageHandler(previousHandler);
    return result;
}
