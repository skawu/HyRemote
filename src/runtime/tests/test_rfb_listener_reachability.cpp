// #274 Phase B4: real listener reachability and interface reconciliation are Shared Runtime/RFB
// integration contracts. They use the private Runtime seam directly so the tests do not depend on the
// Embedded C++ facade. #338's deterministic listener-binding unit test remains complementary: it
// validates pure resolution/mode/locality decisions without opening real listeners.

#include "access_instance.hpp"
#include "access_types.hpp"
#include "detail/listener_binding.hpp"
#include "detail/component_factories.hpp"
#include "hyremote/core/storage.hpp"

#include <QApplication>
#include <QByteArray>
#include <QElapsedTimer>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWidget>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

class PortProbe
{
public:
    bool acquire()
    {
        m_server = std::make_unique<QTcpServer>();
        if (!m_server->listen(QHostAddress::LocalHost, 0))
            return false;
        m_port = m_server->serverPort();
        return m_port != 0;
    }

    void release()
    {
        if (m_server)
            m_server->close();
        m_server.reset();
    }

    quint16 port() const { return m_port; }

private:
    std::unique_ptr<QTcpServer> m_server;
    quint16 m_port = 0;
};

bool dials(const char *address, quint16 port)
{
    QTcpSocket socket;
    socket.connectToHost(QHostAddress(QString::fromLatin1(address)), port);
    const bool connected = socket.waitForConnected(2000);
    socket.abort();
    return connected;
}

// RFB wire-level client: unlike the synthetic transport callbacks in #460,
// this performs the *real* production listener's RFB 3.8 handshake and reads
// one Raw framebuffer update. It is NOT a third-party maintained VNC Viewer,
// a network-shaping test, or a measure of #410 user-perceived latency.
QByteArray readRfbExact(QTcpSocket &socket, qsizetype size, int timeoutMs = 3000)
{
    QByteArray data;
    QElapsedTimer timer;
    timer.start();
    while (data.size() < size && timer.elapsed() < timeoutMs) {
        if (socket.bytesAvailable() == 0) {
            const int remaining = qMax(1, timeoutMs - static_cast<int>(timer.elapsed()));
            if (!socket.waitForReadyRead(remaining))
                break;
        }
        data += socket.read(size - data.size());
    }
    return data;
}

bool sendRfb(QTcpSocket &socket, const QByteArray &data)
{
    if (socket.write(data) != data.size())
        return false;
    socket.flush();
    return socket.bytesToWrite() == 0 || socket.waitForBytesWritten(3000);
}

bool connectRfb38AndReadBootstrap(QTcpSocket &socket, quint16 port)
{
    socket.connectToHost(QHostAddress::LocalHost, port);
    if (!socket.waitForConnected(3000))
        return false;
    const QByteArray version("RFB 003.008\\n", 12);
    if (readRfbExact(socket, 12) != version || !sendRfb(socket, version))
        return false;
    const QByteArray countBytes = readRfbExact(socket, 1);
    if (countBytes.size() != 1)
        return false;
    const int count = static_cast<unsigned char>(countBytes.at(0));
    if (count < 1 || count > 16)
        return false;
    const QByteArray security = readRfbExact(socket, count);
    if (security.size() != count || !security.contains(char(1)))
        return false;
    if (!sendRfb(socket, QByteArray(1, char(1)))
        || readRfbExact(socket, 4) != QByteArray(4, char(0))
        || !sendRfb(socket, QByteArray(1, char(1))))
        return false;

    const QByteArray init = readRfbExact(socket, 24);
    if (init.size() != 24 || static_cast<unsigned char>(init[0]) != 0
        || static_cast<unsigned char>(init[1]) != 2
        || static_cast<unsigned char>(init[2]) != 0
        || static_cast<unsigned char>(init[3]) != 2
        || static_cast<unsigned char>(init[4]) != 32)
        return false;
    const auto byte = [&](int i) { return static_cast<std::uint32_t>(static_cast<unsigned char>(init[i])); };
    const std::uint32_t nameBytes = (byte(20) << 24U) | (byte(21) << 16U)
                                    | (byte(22) << 8U) | byte(23);
    if (nameBytes > 128U || readRfbExact(socket, static_cast<qsizetype>(nameBytes)) != "HyRemote")
        return false;

    // Force a full refresh: a connected client must still obtain the already
    // captured bootstrap framebuffer without depending on another request.
    const QByteArray fullRequest("\\x03\\x00\\x00\\x00\\x00\\x00\\x00\\x02\\x00\\x02", 10);
    if (!sendRfb(socket, fullRequest))
        return false;
    const QByteArray reply = readRfbExact(socket, 4);
    if (reply != QByteArray("\\x00\\x00\\x00\\x01", 4))
        return false;
    const QByteArray rect = readRfbExact(socket, 12);
    if (rect.size() != 12 || static_cast<unsigned char>(rect[5]) != 2
        || static_cast<unsigned char>(rect[7]) != 2
        || rect.mid(8, 4) != QByteArray(4, char(0)))
        return false;
    return readRfbExact(socket, 16).size() == 16;
}

template <typename Predicate>
bool waitForDemand(Predicate condition, int timeoutMs = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return condition();
}

struct CaptureDemandCounters
{
    std::atomic<int> requests{0};
    std::atomic<int> bootstrapFrames{0};
};

// The first request publishes an owned 2x2 BGRA framebuffer; later requests
// are refused so we can count admitted demands without flooding the network.
// Every AccessInstance run constructs a new source with fresh bootstrap state.
class BootstrapProbeCapture final : public hyremote::CaptureSource
{
public:
    explicit BootstrapProbeCapture(std::shared_ptr<CaptureDemandCounters> counters)
        : m_counters(std::move(counters)) {}

    hyremote::CaptureCapabilities capabilities() const override
    {
        hyremote::CaptureCapabilities caps;
        caps.asynchronous = true;
        caps.cpuReadable = true;
        caps.cpuFormats = {hyremote::PixelFormat::Bgra8888};
        return caps;
    }

    bool start(hyremote::FrameReadyHandler onFrame, hyremote::CaptureEventHandler) override
    {
        m_onFrame = std::move(onFrame);
        m_active.store(true, std::memory_order_release);
        m_first = true;
        return true;
    }

    void stop() noexcept override { m_active.store(false, std::memory_order_release); }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        if (!m_active.load(std::memory_order_acquire))
            return false;
        ++m_counters->requests;
        if (!m_first)
            return false;
        m_first = false;
        auto storage = hyremote::CpuFrameStorage::createSinglePlane(8, 2);
        if (!storage || !storage->mutablePlane(0))
            return false;
        std::memset(storage->mutablePlane(0), 0, 16);
        hyremote::RemoteFrame frame;
        frame.geometry.size = {2, 2};
        frame.geometry.pixelFormat = hyremote::PixelFormat::Bgra8888;
        frame.geometry.alphaMode = hyremote::AlphaMode::Premultiplied;
        frame.geometry.planeCount = 1;
        frame.storage = std::move(storage);
        frame.timing.ptsSource = hyremote::PtsSource::Completion;
        frame.timing.requestTime = request.requestTime;
        frame.timing.completionTime = hyremote::Clock::now();
        frame.damage = hyremote::Damage::fullFrame();
        frame.requestId = request.id;
        ++m_counters->bootstrapFrames;
        m_onFrame(std::move(frame));
        return true;
    }

private:
    std::shared_ptr<CaptureDemandCounters> m_counters;
    hyremote::FrameReadyHandler m_onFrame;
    std::atomic<bool> m_active{false};
    bool m_first = true;  // Only touched by the single Core scheduler thread.
};

void testRealRfbConnectionWakesViewerlessCapture()
{
    using HyRemote::Runtime::AccessInstance;
    using HyRemote::Runtime::AccessState;
    PortProbe probe;
    CHECK(probe.acquire());
    const quint16 port = probe.port();
    probe.release();
    if (port == 0)
        return;

    auto counters = std::make_shared<CaptureDemandCounters>();
    HyRemote::detail::setTargetFactory([counters](QObject *target, bool) {
        HyRemote::detail::TargetComponents parts;
        parts.supported = target != nullptr;
        if (parts.supported)
            parts.capture = std::make_unique<BootstrapProbeCapture>(counters);
        return parts;
    });

    QObject target;
    AccessInstance instance(&target);
    CHECK(instance.setListenAddress(QHostAddress::LocalHost));
    CHECK(instance.setPort(port));
    const bool started = instance.start();
    CHECK(started);
    if (started) {
        CHECK(waitForDemand([&] { return counters->bootstrapFrames.load() == 1; }));
        CHECK(instance.connectedClientCount() == 0u);

        // A real listener with no connected viewer stops calling capture
        // after the ServerInit-usable bootstrap frame.
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        const int idle = counters->requests.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        CHECK(counters->requests.load() == idle);

        QTcpSocket first;
        CHECK(connectRfb38AndReadBootstrap(first, port));
        CHECK(waitForDemand([&] { return instance.connectedClientCount() == 1u; }));
        CHECK(waitForDemand([&] { return counters->requests.load() > idle; }));
        first.disconnectFromHost();
        first.waitForDisconnected(3000);
        CHECK(waitForDemand([&] { return instance.connectedClientCount() == 0u; }));

        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        const int disconnected = counters->requests.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        CHECK(counters->requests.load() == disconnected);

        // Restart rebuilds both the single Runtime demand gate and source.
        instance.stop();
        CHECK(instance.state() == AccessState::Stopped);
        CHECK(instance.start());
        CHECK(waitForDemand([&] { return counters->bootstrapFrames.load() == 2; }));
        CHECK(instance.connectedClientCount() == 0u);
        QTcpSocket second;
        const int beforeReconnect = counters->requests.load();
        CHECK(connectRfb38AndReadBootstrap(second, port));
        CHECK(waitForDemand([&] { return instance.connectedClientCount() == 1u; }));
        CHECK(waitForDemand([&] { return counters->requests.load() > beforeReconnect; }));
        second.disconnectFromHost();
        second.waitForDisconnected(3000);
        CHECK(waitForDemand([&] { return instance.connectedClientCount() == 0u; }));
        instance.stop();
        CHECK(instance.state() == AccessState::Stopped);
    }
    HyRemote::detail::resetFactories();
}

struct AddressRow
{
    const char *name;
    const char *address;
    const char *dial;
    bool expectReachable;
};

void measureAddressRow(const AddressRow &row)
{
    using HyRemote::Runtime::AccessInstance;
    using HyRemote::Runtime::AccessState;

    PortProbe probe;
    const bool acquired = probe.acquire();
    CHECK(acquired);
    if (!acquired)
        return;
    const quint16 port = probe.port();
    probe.release();
    CHECK(port != 0);
    if (port == 0)
        return;

    QWidget target;
    AccessInstance instance(&target);
    const QHostAddress address(QString::fromLatin1(row.address));
    CHECK(!address.isNull());
    CHECK(instance.setListenAddress(address));
    CHECK(instance.setPort(port));
    instance.clearError();

    const bool started = instance.start();
    std::cout << "row " << row.name << " (" << row.address << ") started=" << started;
    if (!started) {
        const auto error = instance.lastError();
        std::cout << " message=" << (error ? error->message.toStdString() : std::string("<none>"));
    }
    std::cout << '\n';

    if (row.expectReachable)
        CHECK(started);

    if (started) {
        CHECK(instance.state() == AccessState::Running);
        const bool connected = dials(row.dial, port);
        std::cout << "    reachable via " << row.dial << " = " << connected
                  << " (expected " << row.expectReachable << ")\n";
        CHECK(connected == row.expectReachable);
        instance.stop();
        CHECK(instance.state() == AccessState::Stopped);
    } else {
        CHECK(!dials("127.0.0.1", port));
    }
}

// #338: drive the exact reconciliation entry point used by the Runtime watcher. The injected interface
// snapshot keeps the transition deterministic while reachability is measured on real sockets.
void testInterfaceFollowing()
{
    using HyRemote::Runtime::AccessInstance;
    using HyRemote::Runtime::AccessState;

    const QString identity = QStringLiteral("hyremote-test-iface");
    auto liveAddresses = std::make_shared<QStringList>();

    HyRemote::detail::setInterfaceAddressProvider([identity, liveAddresses](const QString &requested, bool &found) {
        found = (requested == identity);
        QList<QHostAddress> resolved;
        if (!found)
            return resolved;
        for (const QString &candidate : *liveAddresses)
            resolved.append(QHostAddress(candidate));
        return resolved;
    });

    PortProbe probe;
    const bool acquired = probe.acquire();
    CHECK(acquired);
    if (!acquired) {
        HyRemote::detail::resetInterfaceAddressProvider();
        return;
    }
    const quint16 port = probe.port();
    probe.release();
    CHECK(port != 0);
    if (port == 0) {
        HyRemote::detail::resetInterfaceAddressProvider();
        return;
    }

    QWidget target;
    AccessInstance instance(&target);
    CHECK(instance.setPort(port));
    CHECK(instance.setListenInterface(identity));
    CHECK(instance.listenInterface() == identity);

    *liveAddresses = QStringList{QStringLiteral("127.0.0.1")};
    CHECK(instance.start());
    CHECK(instance.state() == AccessState::Running);
    {
        const auto diagnostics = instance.diagnosticSnapshot();
        CHECK(diagnostics.effectiveListenAddress.has_value());
        CHECK(diagnostics.effectiveListenAddress == QHostAddress(QStringLiteral("127.0.0.1")));
        CHECK(diagnostics.effectivePort == port);
    }
    CHECK(dials("127.0.0.1", port));

    *liveAddresses = QStringList{QStringLiteral("127.0.0.2")};
    instance.reconcileInterfaceBinding();
    CHECK(instance.state() == AccessState::Running);
    {
        const auto diagnostics = instance.diagnosticSnapshot();
        CHECK(diagnostics.effectiveListenAddress.has_value());
        CHECK(diagnostics.effectiveListenAddress == QHostAddress(QStringLiteral("127.0.0.2")));
        CHECK(diagnostics.effectivePort == port);
    }
    CHECK(dials("127.0.0.2", port));
    CHECK(!dials("127.0.0.1", port));
    CHECK(instance.listenInterface() == identity);

    liveAddresses->clear();
    instance.reconcileInterfaceBinding();
    CHECK(instance.state() == AccessState::Unavailable);
    {
        const auto diagnostics = instance.diagnosticSnapshot();
        CHECK(!diagnostics.effectiveListenAddress.has_value());
        CHECK(!diagnostics.effectivePort.has_value());
        CHECK(diagnostics.lastError.has_value());
    }
    CHECK(!dials("127.0.0.1", port));
    CHECK(!dials("127.0.0.2", port));
    CHECK(instance.lastError().has_value());
    if (instance.lastError())
        std::cout << "unavailable reason: " << instance.lastError()->message.toStdString() << '\n';

    CHECK(!instance.setListenAddress(QHostAddress(QStringLiteral("127.0.0.1"))));
    CHECK(!instance.setListenInterface(QStringLiteral("some-other-iface")));
    CHECK(!instance.setPort(port == 65000 ? 65001 : 65000));
    CHECK(instance.listenInterface() == identity);

    *liveAddresses = QStringList{QStringLiteral("127.0.0.2")};
    instance.reconcileInterfaceBinding();
    CHECK(instance.state() == AccessState::Running);
    {
        const auto diagnostics = instance.diagnosticSnapshot();
        CHECK(diagnostics.effectiveListenAddress == QHostAddress(QStringLiteral("127.0.0.2")));
        CHECK(diagnostics.effectivePort == port);
        CHECK(!diagnostics.lastError.has_value());
    }
    CHECK(dials("127.0.0.2", port));
    CHECK(!instance.lastError().has_value());

    instance.stop();
    CHECK(instance.state() == AccessState::Stopped);
    {
        const auto diagnostics = instance.diagnosticSnapshot();
        CHECK(!diagnostics.effectiveListenAddress.has_value());
        CHECK(!diagnostics.effectivePort.has_value());
    }
    *liveAddresses = QStringList{QStringLiteral("127.0.0.1")};
    instance.reconcileInterfaceBinding();
    CHECK(instance.state() == AccessState::Stopped);
    CHECK(!dials("127.0.0.1", port));
    CHECK(instance.setListenAddress(QHostAddress(QStringLiteral("127.0.0.1"))));

    HyRemote::detail::resetInterfaceAddressProvider();
}

// Where a host exposes one usable non-loopback IPv4, prove the three shipped binding modes against the
// actual LAN address. A host without such an interface cannot supply this platform fact, so the row reports
// that absence while the deterministic interface-resolution contract remains covered by the Runtime unit test.
void testRealLanInterfaceBinding()
{
    using HyRemote::Runtime::AccessInstance;
    using HyRemote::Runtime::AccessState;

    QString identity;
    QString lanAddress;
    for (const QNetworkInterface &candidate : QNetworkInterface::allInterfaces()) {
        const QNetworkInterface::InterfaceFlags flags = candidate.flags();
        if (flags.testFlag(QNetworkInterface::IsLoopBack) || !flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)) {
            continue;
        }

        QStringList addresses;
        for (const QNetworkAddressEntry &entry : candidate.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (!ip.isNull() && ip.protocol() == QAbstractSocket::IPv4Protocol)
                addresses.append(ip.toString());
        }
        if (addresses.size() == 1) {
            identity = candidate.name();
            lanAddress = addresses.first();
            break;
        }
    }

    if (identity.isEmpty() || lanAddress.isEmpty()) {
        std::cout << "real-LAN rows: unavailable on this host (no non-loopback interface with exactly one IPv4)\n";
        return;
    }

    const QByteArray dialTarget = lanAddress.toLatin1();
    std::cout << "real LAN: interface=" << identity.toStdString() << " address=" << lanAddress.toStdString() << '\n';

    PortProbe probe;
    const bool acquired = probe.acquire();
    CHECK(acquired);
    if (!acquired)
        return;
    const quint16 port = probe.port();
    probe.release();
    CHECK(port != 0);
    if (port == 0)
        return;

    QWidget target;

    {
        AccessInstance instance(&target);
        CHECK(instance.setPort(port));
        CHECK(instance.start());
        CHECK(instance.state() == AccessState::Running);
        CHECK(dials(dialTarget.constData(), port));
        instance.stop();
    }

    {
        AccessInstance instance(&target);
        CHECK(instance.setListenAddress(QHostAddress(lanAddress)));
        CHECK(instance.setPort(port));
        CHECK(instance.start());
        CHECK(instance.state() == AccessState::Running);
        CHECK(dials(dialTarget.constData(), port));
        instance.stop();
    }

    {
        AccessInstance instance(&target);
        CHECK(instance.setListenInterface(identity));
        CHECK(instance.setPort(port));
        CHECK(instance.start());
        CHECK(instance.state() == AccessState::Running);
        CHECK(instance.listenInterface() == identity);
        CHECK(dials(dialTarget.constData(), port));
        instance.stop();
    }
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    const AddressRow rows[] = {
        {"wildcard-ipv4", "0.0.0.0", "127.0.0.1", true},
        {"loopback-ipv4", "127.0.0.1", "127.0.0.1", true},
    };
    for (const AddressRow &row : rows)
        measureAddressRow(row);

    testInterfaceFollowing();
    testRealLanInterfaceBinding();
    testRealRfbConnectionWakesViewerlessCapture();

    std::cout << (failures == 0 ? "PASS: Runtime/RFB listener reachability"
                                : "FAIL: Runtime/RFB listener reachability")
              << " (" << failures << " checks failed)\n";
    return failures == 0 ? 0 : 1;
}
