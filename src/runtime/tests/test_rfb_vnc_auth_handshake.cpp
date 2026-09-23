// #143 authentication step: the handshake has to be measured, not asserted in prose. The unit test for the VNC
// primitive cannot say whether the transport actually offers security type 2, verifies the response, refuses to
// downgrade to None, or reports a failure - so this drives a real socket through the whole exchange.
//
// Every port here is assigned by the operating system: a probe QTcpServer binds port 0 and reports the number it
// got, and the transport is then told to use exactly that number. Nothing in this file picks a port, and no fixed
// port is needed.
#include <QCoreApplication>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include "hyremote/core/transport.hpp"
#include "transport/rfb_transport.hpp"
#include "transport/vnc_auth.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            ++failures;                                                                            \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #expr "\n";             \
        }                                                                                          \
    } while (false)

using HyRemote::detail::RfbSecurityConfig;

quint16 probeFreePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0))
        return 0;
    const quint16 port = probe.serverPort();
    probe.close();
    return port;
}

struct EventRecorder
{
    void record(hyremote::TransportEventCode code)
    {
        std::lock_guard<std::mutex> lock(mutex);
        codes.push_back(code);
    }

    bool saw(hyremote::TransportEventCode code) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return std::find(codes.begin(), codes.end(), code) != codes.end();
    }

    // The event is published by the transport's own thread, and the client can observe the connection closing
    // before that thread has run the handler. Waiting for a bounded time is what makes this deterministic.
    bool waitFor(hyremote::TransportEventCode code, int timeoutMs) const
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (saw(code))
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return saw(code);
    }

    mutable std::mutex mutex;
    std::vector<hyremote::TransportEventCode> codes;
};

bool readExact(QTcpSocket &socket, int count, QByteArray &out)
{
    out.clear();
    while (out.size() < count) {
        if (!socket.waitForReadyRead(5000))
            return false;
        out += socket.read(count - out.size());
    }
    return true;
}

// The security type the server offers for the given configuration, read from a connected client.
bool readOfferedSecurityType(QTcpSocket &socket, std::uint8_t &offered)
{
    QByteArray version;
    if (!readExact(socket, 12, version))
        return false;
    if (!version.startsWith("RFB 003.008"))
        return false;
    socket.write("RFB 003.008\n", 12);
    if (socket.waitForBytesWritten(2000) != true)
        return false;

    QByteArray offer;
    if (!readExact(socket, 2, offer))
        return false;
    if (static_cast<std::uint8_t>(offer.at(0)) != 1)
        return false;  // exactly one type is ever offered
    offered = static_cast<std::uint8_t>(offer.at(1));
    return true;
}

// An already-closed socket is the outcome we want: waitForDisconnected reports false when there was nothing left
// to wait for, which would turn the desired result into a failed assertion.
bool disconnectedWithin(QTcpSocket &socket, int timeoutMs)
{
    if (socket.state() == QAbstractSocket::UnconnectedState)
        return true;
    return socket.waitForDisconnected(timeoutMs);
}

void writeType(QTcpSocket &socket, std::uint8_t type)
{
    socket.write(reinterpret_cast<const char *>(&type), 1);
    socket.waitForBytesWritten(2000);
}

bool readSecurityResult(QTcpSocket &socket, std::uint32_t &result)
{
    QByteArray bytes;
    if (!readExact(socket, 4, bytes))
        return false;
    result = (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes.at(0))) << 24)
             | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes.at(1))) << 16)
             | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes.at(2))) << 8)
             | static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes.at(3)));
    return true;
}

// Starts a transport with the given security configuration on an OS-assigned port and connects a client to it.
struct Fixture
{
    std::unique_ptr<hyremote::Transport> transport;
    std::unique_ptr<QTcpSocket> client;
    std::shared_ptr<EventRecorder> events = std::make_shared<EventRecorder>();
};

bool startFixture(Fixture &fixture, const QByteArray &password, bool authenticationRequired)
{
    const quint16 port = probeFreePort();
    if (port == 0)
        return false;

    RfbSecurityConfig security;
    security.vncAuthenticationRequired = authenticationRequired;
    security.password = password;

    fixture.transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port, security);
    if (!fixture.transport)
        return false;

    std::shared_ptr<EventRecorder> events = fixture.events;
    if (!fixture.transport->start({}, [events](const hyremote::TransportEvent &event) {
            events->record(event.code);
        })) {
        return false;
    }

    // The listener is opened on the worker thread, so the connect is retried until it is up.
    for (int attempt = 0; attempt < 50; ++attempt) {
        auto client = std::make_unique<QTcpSocket>();
        client->connectToHost(QHostAddress::LocalHost, port);
        if (client->waitForConnected(200)) {
            fixture.client = std::move(client);
            return true;
        }
        client->abort();
    }
    return false;
}

void stopFixture(Fixture &fixture)
{
    if (fixture.client)
        fixture.client->abort();
    if (fixture.transport)
        fixture.transport->stop();
}

// A client that selects the offered type and answers the challenge with the response for `password`.
bool completeHandshake(QTcpSocket &socket, const QByteArray &password, std::uint32_t &result)
{
    std::uint8_t offered = 0;
    if (!readOfferedSecurityType(socket, offered))
        return false;
    if (offered != 2)
        return false;
    writeType(socket, 2);

    QByteArray challenge;
    if (!readExact(socket, static_cast<int>(HyRemote::detail::kVncAuthChallengeBytes), challenge))
        return false;

    QByteArray response;
    QString error;
    if (!HyRemote::detail::computeVncAuthResponse(password, challenge, response, error))
        return false;
    socket.write(response);
    socket.waitForBytesWritten(2000);

    return readSecurityResult(socket, result);
}

void testCorrectCredentialSucceeds()
{
    Fixture fixture;
    CHECK(startFixture(fixture, QByteArrayLiteral("s3cret!"), true));

    std::uint32_t result = 0xFFFFFFFFu;
    CHECK(completeHandshake(*fixture.client, QByteArrayLiteral("s3cret!"), result));
    CHECK(result == 0);  // SecurityResult OK

    // Success is not reported as a rejection, and the configured type was the only one offered.
    CHECK(!fixture.events->saw(hyremote::TransportEventCode::AuthenticationRejected));
    stopFixture(fixture);
}

void testWrongCredentialIsRejected()
{
    Fixture fixture;
    CHECK(startFixture(fixture, QByteArrayLiteral("s3cret!"), true));

    std::uint32_t result = 0;
    CHECK(completeHandshake(*fixture.client, QByteArrayLiteral("wr0ng!!!"), result));
    CHECK(result != 0);  // SecurityResult failed

    // The failure is reported through the event reserved for it, and the connection does not stay usable.
    CHECK(fixture.events->waitFor(hyremote::TransportEventCode::AuthenticationRejected, 3000));
    CHECK(disconnectedWithin(*fixture.client, 5000));
    stopFixture(fixture);
}

void testSelectingNoneIsRefused()
{
    Fixture fixture;
    CHECK(startFixture(fixture, QByteArrayLiteral("s3cret!"), true));

    std::uint8_t offered = 0;
    CHECK(readOfferedSecurityType(*fixture.client, offered));
    CHECK(offered == 2);  // None is not on the menu when authentication is configured

    // A client that asks for None anyway is rejected rather than downgraded.
    writeType(*fixture.client, 1);
    CHECK(disconnectedWithin(*fixture.client, 5000));
    stopFixture(fixture);
}

void testStalledClientIsClosedByTheHandshakeBound()
{
    Fixture fixture;
    CHECK(startFixture(fixture, QByteArrayLiteral("s3cret!"), true));

    std::uint8_t offered = 0;
    CHECK(readOfferedSecurityType(*fixture.client, offered));
    CHECK(offered == 2);
    writeType(*fixture.client, 2);

    QByteArray challenge;
    CHECK(readExact(*fixture.client, static_cast<int>(HyRemote::detail::kVncAuthChallengeBytes), challenge));

    // Nothing further is sent: the handshake timeout has to close the connection rather than wait forever.
    CHECK(disconnectedWithin(*fixture.client, 8000));
    stopFixture(fixture);
}

void testInsecureProfileStillOffersNoneOnly()
{
    Fixture fixture;
    CHECK(startFixture(fixture, QByteArray(), false));

    std::uint8_t offered = 0;
    CHECK(readOfferedSecurityType(*fixture.client, offered));
    CHECK(offered == 1);  // the insecure profile keeps the explicit None mode

    writeType(*fixture.client, 1);
    std::uint32_t result = 0xFFFFFFFFu;
    CHECK(readSecurityResult(*fixture.client, result));
    CHECK(result == 0);
    stopFixture(fixture);
}

}  // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);

    testCorrectCredentialSucceeds();
    testWrongCredentialIsRejected();
    testSelectingNoneIsRefused();
    testStalledClientIsClosedByTheHandshakeBound();
    testInsecureProfileStillOffersNoneOnly();

    if (failures != 0) {
        std::cerr << failures << " RFB VNC authentication assertion(s) failed\n";
        return 1;
    }

    std::cout << "PASS: RFB VNC authentication handshake (offer, challenge, rejection, downgrade refusal, bound)\n";
    return 0;
}
