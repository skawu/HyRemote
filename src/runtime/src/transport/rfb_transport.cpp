#include "transport/rfb_transport.hpp"

#include <QAbstractSocket>
#include <QByteArray>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>

#include <functional>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "hyremote/core/storage.hpp"

#ifdef HYREMOTE_HAS_TRANSPORT_SECURITY
// The VNC challenge/response primitive is compiled only when the transport-security capability is available, so the
// handshake includes it under the same condition: without OpenSSL the transport simply has no authentication to
// offer, and RemoteAccess refuses a secure profile before a listener is opened.
#include "vnc_auth.hpp"
#endif

namespace HyRemote::detail {
namespace {

constexpr int kMaxClients = 8;
constexpr int kHandshakeTimeoutMs = 3000;

// The frozen encrypted profile: SecurityType 19 (VeNCrypt), VeNCrypt protocol 0.2 and sub-type X509Vnc 261, then
// TLS >= 1.2 and VNC Authentication inside it. #258 proved these exact values against the maintained viewer
// (TigerVNC 1.16.2) and against TigerVNC's own GnuTLS server, so nothing in this negotiation is approximate.
constexpr std::uint8_t kSecurityTypeNone = 1;
constexpr std::uint8_t kSecurityTypeVncAuth = 2;
constexpr std::uint8_t kSecurityTypeVeNCrypt = 19;
constexpr std::uint32_t kSubTypeX509Vnc = 261;
constexpr std::uint8_t kVeNCryptVersionMajor = 0;
constexpr std::uint8_t kVeNCryptVersionMinor = 2;
// The byte a VeNCrypt client reads before it will create any TLS state; zero would mean "the server failed to
// initialise TLS", so the peer is told to proceed instead.
constexpr char kTlsInitProceed = 1;
constexpr char kVeNCryptVersionUnsupported = static_cast<char>(0xff);
// How long a failing handshake waits for its final SecurityResult to reach the client before the socket is aborted.
constexpr int kHandshakeFlushMs = 1000;
constexpr qsizetype kMaxClientInputBytes = 256 * 1024;
constexpr std::uint16_t kMaxEncodings = 1024;
constexpr std::uint32_t kMaxCutTextBytes = 64 * 1024;
constexpr std::size_t kMaxHeldKeys = 64;
constexpr std::size_t kKeyCodeCount = static_cast<std::size_t>(hyremote::KeyCode::F12) + 1U;
constexpr std::int32_t kEncodingRaw = 0;
constexpr std::int32_t kEncodingDesktopSize = -223;

std::uint8_t byteAt(const QByteArray &data, qsizetype offset)
{
    return static_cast<std::uint8_t>(static_cast<unsigned char>(data.at(offset)));
}

std::uint16_t readU16(const QByteArray &data, qsizetype offset)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(byteAt(data, offset)) << 8U)
                                      | byteAt(data, offset + 1));
}

std::uint32_t readU32(const QByteArray &data, qsizetype offset)
{
    return (static_cast<std::uint32_t>(byteAt(data, offset)) << 24U)
           | (static_cast<std::uint32_t>(byteAt(data, offset + 1)) << 16U)
           | (static_cast<std::uint32_t>(byteAt(data, offset + 2)) << 8U)
           | static_cast<std::uint32_t>(byteAt(data, offset + 3));
}

std::int32_t readS32(const QByteArray &data, qsizetype offset)
{
    return static_cast<std::int32_t>(readU32(data, offset));
}

void appendU16(QByteArray &data, std::uint16_t value)
{
    data.append(static_cast<char>((value >> 8U) & 0xffU));
    data.append(static_cast<char>(value & 0xffU));
}

void appendU32(QByteArray &data, std::uint32_t value)
{
    data.append(static_cast<char>((value >> 24U) & 0xffU));
    data.append(static_cast<char>((value >> 16U) & 0xffU));
    data.append(static_cast<char>((value >> 8U) & 0xffU));
    data.append(static_cast<char>(value & 0xffU));
}

void appendS32(QByteArray &data, std::int32_t value)
{
    appendU32(data, static_cast<std::uint32_t>(value));
}

struct PixelSpec
{
    std::uint8_t bitsPerPixel = 32;
    std::uint8_t depth = 24;
    bool bigEndian = false;
    bool trueColor = true;
    std::uint16_t redMax = 255;
    std::uint16_t greenMax = 255;
    std::uint16_t blueMax = 255;
    std::uint8_t redShift = 0;
    std::uint8_t greenShift = 8;
    std::uint8_t blueShift = 16;
};

PixelSpec nativePixelSpec()
{
    return {};
}

bool pixelSpecSupported(const PixelSpec &spec)
{
    if (!spec.trueColor || (spec.bitsPerPixel != 8 && spec.bitsPerPixel != 16
                            && spec.bitsPerPixel != 32)) {
        return false;
    }
    if (spec.redMax == 0 || spec.greenMax == 0 || spec.blueMax == 0)
        return false;

    const std::uint64_t limit = spec.bitsPerPixel == 32
                                    ? std::numeric_limits<std::uint32_t>::max()
                                    : ((std::uint64_t{1} << spec.bitsPerPixel) - 1U);
    const auto fits = [limit](std::uint16_t max, std::uint8_t shift) {
        return shift < 32U && (static_cast<std::uint64_t>(max) << shift) <= limit;
    };
    return fits(spec.redMax, spec.redShift) && fits(spec.greenMax, spec.greenShift)
           && fits(spec.blueMax, spec.blueShift);
}

void appendPixelSpec(QByteArray &data, const PixelSpec &spec)
{
    data.append(static_cast<char>(spec.bitsPerPixel));
    data.append(static_cast<char>(spec.depth));
    data.append(static_cast<char>(spec.bigEndian ? 1 : 0));
    data.append(static_cast<char>(spec.trueColor ? 1 : 0));
    appendU16(data, spec.redMax);
    appendU16(data, spec.greenMax);
    appendU16(data, spec.blueMax);
    data.append(static_cast<char>(spec.redShift));
    data.append(static_cast<char>(spec.greenShift));
    data.append(static_cast<char>(spec.blueShift));
    data.append("\0\0\0", 3);
}

std::optional<PixelSpec> parsePixelSpec(const QByteArray &message)
{
    if (message.size() < 20)
        return std::nullopt;
    PixelSpec spec;
    spec.bitsPerPixel = byteAt(message, 4);
    spec.depth = byteAt(message, 5);
    spec.bigEndian = byteAt(message, 6) != 0;
    spec.trueColor = byteAt(message, 7) != 0;
    spec.redMax = readU16(message, 8);
    spec.greenMax = readU16(message, 10);
    spec.blueMax = readU16(message, 12);
    spec.redShift = byteAt(message, 14);
    spec.greenShift = byteAt(message, 15);
    spec.blueShift = byteAt(message, 16);
    if (!pixelSpecSupported(spec))
        return std::nullopt;
    return spec;
}

std::uint32_t scaleChannel(std::uint8_t value, std::uint16_t max)
{
    return (static_cast<std::uint32_t>(value) * static_cast<std::uint32_t>(max) + 127U) / 255U;
}

void appendEncodedPixel(QByteArray &out,
                        std::uint8_t r,
                        std::uint8_t g,
                        std::uint8_t b,
                        const PixelSpec &spec)
{
    const std::uint32_t value = (scaleChannel(r, spec.redMax) << spec.redShift)
                                | (scaleChannel(g, spec.greenMax) << spec.greenShift)
                                | (scaleChannel(b, spec.blueMax) << spec.blueShift);
    const int bytesPerPixel = spec.bitsPerPixel / 8;
    if (spec.bigEndian) {
        for (int i = bytesPerPixel - 1; i >= 0; --i)
            out.append(static_cast<char>((value >> (i * 8)) & 0xffU));
    } else {
        for (int i = 0; i < bytesPerPixel; ++i)
            out.append(static_cast<char>((value >> (i * 8)) & 0xffU));
    }
}

hyremote::KeyCode keyCodeFromKeysym(std::uint32_t keysym)
{
    using hyremote::KeyCode;
    switch (keysym) {
    case 0xff0d:
        return KeyCode::Enter;
    case 0xff1b:
        return KeyCode::Escape;
    case 0xff09:
        return KeyCode::Tab;
    case 0xff08:
        return KeyCode::Backspace;
    case 0xffff:
        return KeyCode::DeleteForward;
    case 0xff63:
        return KeyCode::Insert;
    case 0xff50:
        return KeyCode::Home;
    case 0xff57:
        return KeyCode::End;
    case 0xff55:
        return KeyCode::PageUp;
    case 0xff56:
        return KeyCode::PageDown;
    case 0xff51:
        return KeyCode::ArrowLeft;
    case 0xff52:
        return KeyCode::ArrowUp;
    case 0xff53:
        return KeyCode::ArrowRight;
    case 0xff54:
        return KeyCode::ArrowDown;
    case 0x20:
        return KeyCode::Space;
    case 0xffe1:
    case 0xffe2:
        return KeyCode::Shift;
    case 0xffe3:
    case 0xffe4:
        return KeyCode::Control;
    case 0xffe9:
    case 0xffea:
        return KeyCode::Alt;
    case 0xffe7:
    case 0xffe8:
        return KeyCode::Meta;
    case 0xffe5:
        return KeyCode::CapsLock;
    case 0xff7f:
        return KeyCode::NumLock;
    default:
        break;
    }

    if (keysym >= '0' && keysym <= '9')
        return static_cast<KeyCode>(static_cast<int>(KeyCode::Digit0) + keysym - '0');
    if (keysym >= 'A' && keysym <= 'Z')
        return static_cast<KeyCode>(static_cast<int>(KeyCode::A) + keysym - 'A');
    if (keysym >= 'a' && keysym <= 'z')
        return static_cast<KeyCode>(static_cast<int>(KeyCode::A) + keysym - 'a');
    if (keysym >= 0xffbe && keysym <= 0xffc9)
        return static_cast<KeyCode>(static_cast<int>(KeyCode::F1) + keysym - 0xffbe);
    return KeyCode::Unknown;
}

std::optional<hyremote::InputModifier> modifierForKey(hyremote::KeyCode key)
{
    using hyremote::InputModifier;
    using hyremote::KeyCode;
    switch (key) {
    case KeyCode::Shift:
        return InputModifier::Shift;
    case KeyCode::Control:
        return InputModifier::Control;
    case KeyCode::Alt:
        return InputModifier::Alt;
    case KeyCode::Meta:
        return InputModifier::Meta;
    case KeyCode::CapsLock:
        return InputModifier::CapsLock;
    case KeyCode::NumLock:
        return InputModifier::NumLock;
    default:
        return std::nullopt;
    }
}

hyremote::InputModifiers modifiersForHeldKeysyms(const std::vector<std::uint32_t> &heldKeysyms)
{
    hyremote::InputModifiers result = 0U;
    for (const std::uint32_t keysym : heldKeysyms) {
        if (const auto modifier = modifierForKey(keyCodeFromKeysym(keysym)))
            result |= hyremote::modifierMask(*modifier);
    }
    return result;
}

std::string utf8ForKeysym(std::uint32_t keysym)
{
    std::uint32_t cp = 0;
    if (keysym >= 0x20 && keysym <= 0x7e)
        cp = keysym;
    else if (keysym >= 0xa0 && keysym <= 0xff)
        cp = keysym;
    else if ((keysym & 0xff000000U) == 0x01000000U)
        cp = keysym & 0x00ffffffU;
    else
        return {};

    if (cp > 0x10ffffU || (cp >= 0xd800U && cp <= 0xdfffU))
        return {};

    std::string out;
    if (cp <= 0x7fU) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7ffU) {
        out.push_back(static_cast<char>(0xc0U | (cp >> 6U)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3fU)));
    } else if (cp <= 0xffffU) {
        out.push_back(static_cast<char>(0xe0U | (cp >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3fU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3fU)));
    } else {
        out.push_back(static_cast<char>(0xf0U | (cp >> 18U)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 12U) & 0x3fU)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3fU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3fU)));
    }
    return out;
}

struct SharedFrameState
{
    std::mutex mutex;
    std::optional<hyremote::RemoteFrame> latest;
    bool notificationPending = false;
};

enum class ClientPhase {
    AwaitVersion,
    AwaitSecurityChoice,
    // Encrypted profile only: VeNCrypt's own version handshake, its sub-type choice, and the TLS handshake that
    // carries the rest of the RFB session once the sub-type is accepted.
    AwaitVeNCryptVersion,
    AwaitVeNCryptSubtype,
    AwaitTlsHandshake,
    AwaitAuthResponse,
    AwaitClientInit,
    AwaitInitialFrame,
    Normal,
};

struct ClientState
{
    // Opaque, run-local identity of one accepted peer. It exists so the later session layer (#170) can correlate a
    // connection without ever seeing a socket, a file descriptor or a backend pointer. It is not a product session
    // identity: it is assigned at accept, invalidated at disconnect and never reused across connections.
    std::uint64_t peerToken = 0;
    QPointer<QTcpSocket> socket;
    ClientPhase phase = ClientPhase::AwaitVersion;
    QByteArray input;
    // The fresh random challenge this connection was given, kept only until its response is verified.
    QByteArray authChallenge;
    PixelSpec pixels = nativePixelSpec();
    bool supportsDesktopSize = false;
    bool updateRequested = false;
    bool incrementalRequest = false;
    std::uint16_t requestX = 0;
    std::uint16_t requestY = 0;
    std::uint16_t requestWidth = 0;
    std::uint16_t requestHeight = 0;
    std::uint16_t framebufferWidth = 0;
    std::uint16_t framebufferHeight = 0;
    std::uint8_t buttonMask = 0;
    hyremote::InputModifiers modifiers = 0U;
    std::vector<std::uint32_t> heldKeysyms;
    hyremote::InputViewport lastPointerViewport;
    std::uint16_t lastPointerX = 0;
    std::uint16_t lastPointerY = 0;
    bool pointerPositionKnown = false;
    bool connectedEventSent = false;
};

struct ButtonBit
{
    std::uint8_t bit;
    hyremote::PointerButton button;
};

static constexpr std::array<ButtonBit, 3> kButtons{{
    {0x01, hyremote::PointerButton::Left},
    {0x02, hyremote::PointerButton::Middle},
    {0x04, hyremote::PointerButton::Right},
}};

// The accepted-socket model the preflight froze: the transport owns the descriptor from the moment it is
// accepted, and for the encrypted profile it owns it as a QSslSocket for the whole connection. A plain QTcpSocket
// is never migrated into a QSslSocket and the session is never re-established on a second connection, so the
// plaintext negotiation and the encrypted session are provably the same connection.
class RfbServer final : public QTcpServer
{
public:
    using DescriptorHandler = std::function<void(qintptr)>;

    RfbServer(DescriptorHandler onDescriptor, QObject *parent)
        : QTcpServer(parent)
        , m_onDescriptor(std::move(onDescriptor))
    {
    }

protected:
    void incomingConnection(qintptr socketDescriptor) override
    {
        if (m_onDescriptor)
            m_onDescriptor(socketDescriptor);
    }

private:
    DescriptorHandler m_onDescriptor;
};

class RfbWorker final : public QObject
{
public:
    RfbWorker(QHostAddress address,
              quint16 port,
              RfbSecurityConfig security,
              std::shared_ptr<SharedFrameState> frames,
              hyremote::InputHandler onInput,
              hyremote::TransportEventHandler onEvent)
        : m_address(std::move(address))
        , m_port(port)
        , m_security(std::move(security))
        , m_frames(std::move(frames))
        , m_onInput(std::move(onInput))
        , m_onEvent(std::move(onEvent))
    {
    }

    bool startServer()
    {
        if (m_server)
            return false;
        m_server = new RfbServer([this](qintptr descriptor) { adoptPendingClient(descriptor); }, this);
        m_server->setMaxPendingConnections(kMaxClients);
        if (!m_server->listen(m_address, m_port)) {
            m_server->deleteLater();
            m_server = nullptr;
            return false;
        }
        return true;
    }

    void shutdown()
    {
        m_stopping = true;
        if (m_server)
            m_server->close();

        for (auto &entry : m_clients) {
            QTcpSocket *socket = entry.first;
            QObject::disconnect(socket, nullptr, this, nullptr);
            socket->abort();
            socket->deleteLater();
        }
        m_clients.clear();
        m_keyHolderCounts.fill(0);
        m_buttonHolderCounts.fill(0);
        m_onInput = {};
        m_onEvent = {};
    }

    void frameAvailable()
    {
        {
            std::lock_guard<std::mutex> lock(m_frames->mutex);
            m_frames->notificationPending = false;
        }

        for (auto &entry : m_clients) {
            ClientState &client = *entry.second;
            if (client.phase == ClientPhase::AwaitInitialFrame)
                sendServerInit(client);
            if (client.phase == ClientPhase::Normal)
                trySendUpdate(client);
        }
    }

private:
    void publishEvent(hyremote::TransportEventCode code, const std::string &message)
    {
        if (m_stopping || !m_onEvent)
            return;
        try {
            m_onEvent(hyremote::TransportEvent{code, message});
        } catch (...) {
            // Transport callbacks are a boundary. Never unwind an application/Core exception into
            // the Qt network event loop.
        }
    }

    void publishInput(const hyremote::InputEvent &event)
    {
        if (m_stopping || !m_onInput)
            return;
        try {
            m_onInput(event);
        } catch (...) {
            publishEvent(hyremote::TransportEventCode::RecoverableFailure,
                         "remote input callback failed at the transport boundary");
        }
    }

    std::optional<hyremote::RemoteFrame> latestFrame() const
    {
        std::lock_guard<std::mutex> lock(m_frames->mutex);
        return m_frames->latest;
    }

    hyremote::InputModifiers aggregateModifiers() const
    {
        hyremote::InputModifiers result = 0U;
        const auto include = [this, &result](hyremote::KeyCode key, hyremote::InputModifier modifier) {
            if (m_keyHolderCounts[static_cast<std::size_t>(key)] != 0U)
                result |= hyremote::modifierMask(modifier);
        };
        include(hyremote::KeyCode::Shift, hyremote::InputModifier::Shift);
        include(hyremote::KeyCode::Control, hyremote::InputModifier::Control);
        include(hyremote::KeyCode::Alt, hyremote::InputModifier::Alt);
        include(hyremote::KeyCode::Meta, hyremote::InputModifier::Meta);
        include(hyremote::KeyCode::CapsLock, hyremote::InputModifier::CapsLock);
        include(hyremote::KeyCode::NumLock, hyremote::InputModifier::NumLock);
        return result;
    }

#ifdef HYREMOTE_HAS_TRANSPORT_SECURITY
    void configureServerTls(QSslSocket &socket)
    {
        // The certificate, the key and the TLS floor all come from the pre-listen preparation: a listener only
        // exists once that material was parsed and proved to match, so nothing here can fail on a bad pair.
        QSslConfiguration configuration = socket.sslConfiguration();
        configuration.setLocalCertificate(m_security.certificate);
        configuration.setPrivateKey(m_security.privateKey);
        configuration.setProtocol(QSsl::TlsV1_2OrLater);
        // A VNC viewer presents no client certificate; the authentication this profile requires is VNC
        // Authentication inside the encrypted channel.
        configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
        socket.setSslConfiguration(configuration);
    }

    void onTlsEncrypted(QSslSocket *socket)
    {
        const auto it = m_clients.find(socket);
        if (it == m_clients.end() || it->second->phase != ClientPhase::AwaitTlsHandshake)
            return;
        ClientState &client = *it->second;

        // Defence in depth: the configuration already requires TLS >= 1.2, and a session negotiated below the
        // release's floor is refused here instead of continuing on a protocol V0.2 does not qualify.
        const QSsl::SslProtocol negotiated = socket->sessionProtocol();
        if (negotiated == QSsl::TlsV1_0 || negotiated == QSsl::TlsV1_1) {
            authenticationRejected(client, "RFB client negotiated a TLS version below the required minimum");
            return;
        }

        // Only now, inside the encrypted channel, is the VNC challenge generated and sent. No authentication byte
        // has crossed this connection before this point, so there is nothing a downgrade attempt could reuse.
        QString challengeError;
        if (!generateVncAuthChallenge(client.authChallenge, challengeError)) {
            protocolFailure(client, "the authentication challenge could not be generated");
            return;
        }
        socket->write(client.authChallenge);
        client.phase = ClientPhase::AwaitAuthResponse;
    }
#endif  // HYREMOTE_HAS_TRANSPORT_SECURITY

    void adoptPendingClient(qintptr descriptor)
    {
        if (static_cast<int>(m_clients.size()) >= kMaxClients) {
            publishEvent(hyremote::TransportEventCode::RecoverableFailure,
                         "RFB client rejected because the bounded client limit was reached");
            QTcpSocket rejected;
            if (rejected.setSocketDescriptor(descriptor))
                rejected.abort();
            return;
        }

        QTcpSocket *socket = nullptr;
#ifdef HYREMOTE_HAS_TRANSPORT_SECURITY
        if (m_security.profile == RfbSecurityProfile::VeNCryptTlsVncAuth) {
            auto *secureSocket = new QSslSocket(this);
            if (!secureSocket->setSocketDescriptor(descriptor)) {
                delete secureSocket;
                publishEvent(hyremote::TransportEventCode::RecoverableFailure,
                             "RFB client connection could not be adopted");
                return;
            }
            configureServerTls(*secureSocket);
            connect(secureSocket, &QSslSocket::encrypted, this, [this, secureSocket] {
                onTlsEncrypted(secureSocket);
            });
            socket = secureSocket;
        } else
#endif
        {
            auto *plainSocket = new QTcpSocket(this);
            if (!plainSocket->setSocketDescriptor(descriptor)) {
                delete plainSocket;
                publishEvent(hyremote::TransportEventCode::RecoverableFailure,
                             "RFB client connection could not be adopted");
                return;
            }
            socket = plainSocket;
        }

        {
            socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
            auto client = std::make_unique<ClientState>();
            client->socket = socket;
            ClientState *clientPtr = client.get();
            m_clients.emplace(socket, std::move(client));

            connect(socket, &QTcpSocket::readyRead, this, [this, socket] { readClient(socket); });
            connect(socket, &QTcpSocket::bytesWritten, this, [this, socket](qint64) {
                const auto it = m_clients.find(socket);
                if (it != m_clients.end())
                    trySendUpdate(*it->second);
            });
            connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                const auto it = m_clients.find(socket);
                if (it == m_clients.end())
                    return;
                const bool announced = it->second->connectedEventSent;
                if (announced)
                    releaseHeldInput(*it->second);
                m_clients.erase(it);
                socket->deleteLater();
                if (announced)
                    publishEvent(hyremote::TransportEventCode::ClientDisconnected,
                                 "RFB client disconnected");
            });

            QPointer<QTcpSocket> guardedSocket(socket);
            QTimer::singleShot(kHandshakeTimeoutMs, this, [this, guardedSocket] {
                QTcpSocket *timedOut = guardedSocket.data();
                if (!timedOut)
                    return;
                const auto found = m_clients.find(timedOut);
                if (found == m_clients.end() || found->second->phase == ClientPhase::Normal)
                    return;
                publishEvent(hyremote::TransportEventCode::RecoverableFailure,
                             "RFB client handshake timed out");
                timedOut->abort();
            });

            static constexpr char kVersion[] = "RFB 003.008\n";
            socket->write(kVersion, 12);
            clientPtr->phase = ClientPhase::AwaitVersion;
        }
    }

    void deferClientAbort(ClientState &client)
    {
        if (!client.socket)
            return;

        // A socket abort dispatches disconnected() synchronously, and that handler erases the ClientState
        // from m_clients. Aborting from processClient() would therefore destroy the state while its caller still
        // holds a reference. Every handshake/protocol rejection uses the same deferred disconnect primitive so
        // the event classification is independent from the lifetime/UAF protection.
        QPointer<QTcpSocket> guardedSocket(client.socket);
        QMetaObject::invokeMethod(
            this,
            [this, guardedSocket] {
                QTcpSocket *socket = guardedSocket.data();
                if (!socket)
                    return;
                if (m_clients.find(socket) == m_clients.end())
                    return;
                socket->abort();
            },
            Qt::QueuedConnection);
    }

    void protocolFailure(ClientState &client, const char *message)
    {
        publishEvent(hyremote::TransportEventCode::RecoverableFailure, message);
        deferClientAbort(client);
    }

    void authenticationRejected(ClientState &client, const char *message)
    {
        // Authentication policy/credential refusal is one transport event, not a generic protocol failure plus a
        // second rejection. #170 consumes this exact event class for session/operator observability later.
        publishEvent(hyremote::TransportEventCode::AuthenticationRejected, message);
        deferClientAbort(client);
    }

    void readClient(QTcpSocket *socket)
    {
        const auto it = m_clients.find(socket);
        if (it == m_clients.end())
            return;
        ClientState &client = *it->second;
        client.input += socket->readAll();
        if (client.input.size() > kMaxClientInputBytes) {
            protocolFailure(client, "RFB client input exceeded the bounded protocol buffer");
            return;
        }
        processClient(client);
    }

    void processClient(ClientState &client)
    {
        for (;;) {
            if (!client.socket)
                return;

            if (client.phase == ClientPhase::AwaitVersion) {
                if (client.input.size() < 12)
                    return;
                const QByteArray version = client.input.left(12);
                client.input.remove(0, 12);
                if (!version.startsWith("RFB 003.00") || version.at(11) != '\n'
                    || (version.mid(8, 3) != "008" && version.mid(8, 3) != "007")) {
                    protocolFailure(client, "RFB client protocol version is unsupported; 3.7/3.8 required");
                    return;
                }
                // Exactly one type is offered, and it is the configured one: a client is never invited to pick a
                // weaker mode, and 'None' is only ever offered when the insecure profile asked for it explicitly.
                // The RFB 3.8 form is a count followed by the type values.
#ifdef HYREMOTE_HAS_TRANSPORT_SECURITY
                std::uint8_t offeredType = kSecurityTypeNone;
                if (m_security.profile == RfbSecurityProfile::VncAuth)
                    offeredType = kSecurityTypeVncAuth;
                else if (m_security.profile == RfbSecurityProfile::VeNCryptTlsVncAuth)
                    offeredType = kSecurityTypeVeNCrypt;
                const char security[] = {1, static_cast<char>(offeredType)};
#else
                const char security[] = {1, 1};  // one type: None; this build has no authentication to offer
#endif
                client.socket->write(security, 2);
                client.phase = ClientPhase::AwaitSecurityChoice;
                continue;
            }

            if (client.phase == ClientPhase::AwaitSecurityChoice) {
                if (client.input.size() < 1)
                    return;
                const std::uint8_t selected = byteAt(client.input, 0);
                client.input.remove(0, 1);
#ifdef HYREMOTE_HAS_TRANSPORT_SECURITY
                if (m_security.profile == RfbSecurityProfile::VeNCryptTlsVncAuth) {
                    if (selected != kSecurityTypeVeNCrypt) {
                        // Never downgrade: a client that will not take the only offered type is refused rather
                        // than invited to continue on a weaker one.
                        authenticationRejected(client,
                                               "RFB client did not select the configured VeNCrypt profile");
                        return;
                    }
                    const char version[] = {static_cast<char>(kVeNCryptVersionMajor),
                                            static_cast<char>(kVeNCryptVersionMinor)};
                    client.socket->write(version, 2);
                    client.phase = ClientPhase::AwaitVeNCryptVersion;
                    continue;
                }

                if (m_security.profile == RfbSecurityProfile::VncAuth) {
                    if (selected != 2) {
                        // Never downgrade: a connection refusing the only configured authentication type is an
                        // authentication rejection, not a generic protocol diagnostic.
                        authenticationRejected(client,
                                               "RFB client did not select the configured authentication type");
                        return;
                    }
                    QString challengeError;
                    if (!generateVncAuthChallenge(client.authChallenge, challengeError)) {
                        protocolFailure(client, "the authentication challenge could not be generated");
                        return;
                    }
                    client.socket->write(client.authChallenge);
                    client.phase = ClientPhase::AwaitAuthResponse;
                    return;
                }
#endif
                if (selected != 1) {
                    protocolFailure(client, "RFB client rejected the supported security type");
                    return;
                }
                QByteArray result;
                appendU32(result, 0);  // SecurityResult OK
                client.socket->write(result);
                client.phase = ClientPhase::AwaitClientInit;
                continue;
            }

#ifdef HYREMOTE_HAS_TRANSPORT_SECURITY
            if (client.phase == ClientPhase::AwaitVeNCryptVersion) {
                if (client.input.size() < 2)
                    return;
                const std::uint8_t major = byteAt(client.input, 0);
                client.input.remove(0, 2);
                if (major != kVeNCryptVersionMajor) {
                    // The client is told with the single byte the profile reserves for this, and the connection
                    // ends here rather than falling back to a weaker type.
                    const char unsupported = kVeNCryptVersionUnsupported;
                    client.socket->write(&unsupported, 1);
                    client.socket->flush();
                    authenticationRejected(client, "RFB client does not support VeNCrypt 0.2");
                    return;
                }

                const char versionAck = 0;
                client.socket->write(&versionAck, 1);
                const char subTypeCount = 1;
                client.socket->write(&subTypeCount, 1);
                QByteArray subType(4, '\0');
                subType[0] = char((kSubTypeX509Vnc >> 24) & 0xff);
                subType[1] = char((kSubTypeX509Vnc >> 16) & 0xff);
                subType[2] = char((kSubTypeX509Vnc >> 8) & 0xff);
                subType[3] = char(kSubTypeX509Vnc & 0xff);
                client.socket->write(subType);
                client.phase = ClientPhase::AwaitVeNCryptSubtype;
                continue;
            }

            if (client.phase == ClientPhase::AwaitVeNCryptSubtype) {
                if (client.input.size() < 4)
                    return;
                const std::uint32_t chosen = (std::uint32_t(byteAt(client.input, 0)) << 24)
                        | (std::uint32_t(byteAt(client.input, 1)) << 16)
                        | (std::uint32_t(byteAt(client.input, 2)) << 8)
                        | std::uint32_t(byteAt(client.input, 3));
                client.input.remove(0, 4);
                if (chosen != kSubTypeX509Vnc) {
                    authenticationRejected(client, "RFB client did not select the X509Vnc VeNCrypt sub-type");
                    return;
                }

                auto *secureSocket = qobject_cast<QSslSocket *>(client.socket.data());
                if (!secureSocket) {
                    protocolFailure(client, "the encrypted profile requires a TLS-capable connection");
                    return;
                }

                // The one byte the client reads before it will create any TLS state. Everything above this line was
                // plaintext on this same connection; everything below is inside the encrypted channel, on the same
                // socket object and the same accepted descriptor.
                const char proceed = kTlsInitProceed;
                client.socket->write(&proceed, 1);
                client.socket->flush();
                client.phase = ClientPhase::AwaitTlsHandshake;
                secureSocket->startServerEncryption();
                return;
            }

            if (client.phase == ClientPhase::AwaitTlsHandshake)
                return;  // bounded by the handshake timeout armed at accept

            if (client.phase == ClientPhase::AwaitAuthResponse) {
                // Bounded by construction: one challenge per connection, no retry, and the 16-byte read is the
                // only thing this phase waits for - the handshake timeout closes a client that stalls.
                if (client.input.size() < kVncAuthChallengeBytes)
                    return;
                const QByteArray response = client.input.left(kVncAuthChallengeBytes);
                client.input.remove(0, kVncAuthChallengeBytes);

                QString verifyError;
                if (!verifyVncAuthResponse(m_security.password, client.authChallenge, response, verifyError)) {
                    QByteArray failed;
                    appendU32(failed, 1);  // SecurityResult failed: the client is told, and not asked to retry
                    if (client.socket) {
                        client.socket->write(failed);
                        // The result has to reach the client before the connection goes away, or the peer sees an
                        // abrupt reset instead of the protocol's own failure answer. Bounded, and only on the
                        // failure path, which ends this connection anyway.
                        client.socket->flush();
                        client.socket->waitForBytesWritten(kHandshakeFlushMs);
                    }
                    client.authChallenge.clear();
                    authenticationRejected(client,
                                           "RFB client failed the configured authentication");
                    return;
                }

                client.authChallenge.clear();
                QByteArray result;
                appendU32(result, 0);  // SecurityResult OK
                client.socket->write(result);
                client.phase = ClientPhase::AwaitClientInit;
                continue;
            }
#endif

            if (client.phase == ClientPhase::AwaitClientInit) {
                if (client.input.size() < 1)
                    return;
                client.input.remove(0, 1);  // shared flag; HyRemote always allows bounded sharing
                if (latestFrame())
                    sendServerInit(client);
                else
                    client.phase = ClientPhase::AwaitInitialFrame;
                continue;
            }

            if (client.phase == ClientPhase::AwaitInitialFrame)
                return;

            if (client.input.isEmpty())
                return;

            const std::uint8_t type = byteAt(client.input, 0);
            if (type == 0) {  // SetPixelFormat
                if (client.input.size() < 20)
                    return;
                const QByteArray message = client.input.left(20);
                client.input.remove(0, 20);
                const auto spec = parsePixelSpec(message);
                if (!spec) {
                    protocolFailure(client, "RFB client requested an unsupported pixel format");
                    return;
                }
                client.pixels = *spec;
                continue;
            }

            if (type == 2) {  // SetEncodings
                if (client.input.size() < 4)
                    return;
                const std::uint16_t count = readU16(client.input, 2);
                if (count > kMaxEncodings) {
                    protocolFailure(client, "RFB client advertised too many encodings");
                    return;
                }
                const qsizetype size = 4 + static_cast<qsizetype>(count) * 4;
                if (client.input.size() < size)
                    return;
                client.supportsDesktopSize = false;
                for (std::uint16_t i = 0; i < count; ++i) {
                    if (readS32(client.input, 4 + static_cast<qsizetype>(i) * 4)
                        == kEncodingDesktopSize) {
                        client.supportsDesktopSize = true;
                    }
                }
                client.input.remove(0, size);
                continue;
            }

            if (type == 3) {  // FramebufferUpdateRequest
                if (client.input.size() < 10)
                    return;
                client.incrementalRequest = byteAt(client.input, 1) != 0;
                client.requestX = readU16(client.input, 2);
                client.requestY = readU16(client.input, 4);
                client.requestWidth = readU16(client.input, 6);
                client.requestHeight = readU16(client.input, 8);
                client.updateRequested = true;  // coalesced: never one queued work item per request
                client.input.remove(0, 10);
                trySendUpdate(client);
                continue;
            }

            if (type == 4) {  // KeyEvent
                if (client.input.size() < 8)
                    return;
                const bool pressed = byteAt(client.input, 1) != 0;
                const std::uint32_t keysym = readU32(client.input, 4);
                client.input.remove(0, 8);
                if (!deliverKey(client, keysym, pressed))
                    return;
                continue;
            }

            if (type == 5) {  // PointerEvent
                if (client.input.size() < 6)
                    return;
                const std::uint8_t mask = byteAt(client.input, 1);
                const std::uint16_t x = readU16(client.input, 2);
                const std::uint16_t y = readU16(client.input, 4);
                client.input.remove(0, 6);
                deliverPointer(client, mask, x, y);
                continue;
            }

            if (type == 6) {  // ClientCutText; deliberately ignored but consumed safely
                if (client.input.size() < 8)
                    return;
                const std::uint32_t length = readU32(client.input, 4);
                if (length > kMaxCutTextBytes) {
                    protocolFailure(client, "RFB client cut-text payload exceeded the bounded limit");
                    return;
                }
                const qsizetype size = 8 + static_cast<qsizetype>(length);
                if (client.input.size() < size)
                    return;
                client.input.remove(0, size);
                continue;
            }

            protocolFailure(client, "RFB client sent an unsupported protocol message");
            return;
        }
    }

    void sendServerInit(ClientState &client)
    {
        const auto frame = latestFrame();
        if (!frame || !client.socket)
            return;
        const auto width = frame->geometry.size.width;
        const auto height = frame->geometry.size.height;
        if (width == 0 || height == 0 || width > std::numeric_limits<std::uint16_t>::max()
            || height > std::numeric_limits<std::uint16_t>::max()) {
            protocolFailure(client, "RFB framebuffer geometry exceeds the protocol baseline");
            return;
        }

        QByteArray init;
        appendU16(init, static_cast<std::uint16_t>(width));
        appendU16(init, static_cast<std::uint16_t>(height));
        appendPixelSpec(init, nativePixelSpec());
        static const QByteArray name("HyRemote");
        appendU32(init, static_cast<std::uint32_t>(name.size()));
        init += name;
        client.socket->write(init);
        client.framebufferWidth = static_cast<std::uint16_t>(width);
        client.framebufferHeight = static_cast<std::uint16_t>(height);
        client.phase = ClientPhase::Normal;
        client.connectedEventSent = true;
        publishEvent(hyremote::TransportEventCode::ClientConnected, "RFB client connected");
    }

    void releaseHeldInput(ClientState &client)
    {
        if (m_stopping || !client.connectedEventSent)
            return;

        // Each client owns only its contribution to the shared target's logical held state. A
        // disconnect decrements those contributions, but it may publish a button release only when
        // that client was the last holder of the logical button.
        if (client.pointerPositionKnown && client.buttonMask != 0
            && hyremote::isValidInputViewport(client.lastPointerViewport)) {
            for (std::size_t index = 0; index < kButtons.size(); ++index) {
                const ButtonBit &entry = kButtons[index];
                if ((client.buttonMask & entry.bit) == 0)
                    continue;
                if (m_buttonHolderCounts[index] == 0U)
                    continue;
                --m_buttonHolderCounts[index];
                if (m_buttonHolderCounts[index] != 0U)
                    continue;

                hyremote::InputEvent button;
                button.kind = hyremote::InputEventKind::PointerButton;
                button.sourceViewport = client.lastPointerViewport;
                button.x = static_cast<float>(client.lastPointerX);
                button.y = static_cast<float>(client.lastPointerY);
                button.button = entry.button;
                button.pressed = false;
                button.modifiers = aggregateModifiers();
                publishInput(button);
            }
        }
        client.buttonMask = 0;

        // Release ordinary keys before modifiers so combinations such as Shift+A preserve the same
        // modifier state on A-up that a normal viewer would have sent. deliverKey() removes only
        // this client's holder contribution and emits the logical release only for the last holder.
        const std::vector<std::uint32_t> held = client.heldKeysyms;
        for (const std::uint32_t keysym : held) {
            if (!modifierForKey(keyCodeFromKeysym(keysym)))
                (void)deliverKey(client, keysym, false);
        }
        const std::vector<std::uint32_t> remaining = client.heldKeysyms;
        for (const std::uint32_t keysym : remaining)
            (void)deliverKey(client, keysym, false);

        client.heldKeysyms.clear();
        client.modifiers = 0U;
    }

    bool deliverKey(ClientState &client, std::uint32_t keysym, bool pressed)
    {
        const hyremote::KeyCode key = keyCodeFromKeysym(keysym);
        bool publishTransition = true;

        if (key != hyremote::KeyCode::Unknown) {
            const auto held = std::find(client.heldKeysyms.begin(), client.heldKeysyms.end(), keysym);
            std::size_t &holderCount = m_keyHolderCounts[static_cast<std::size_t>(key)];

            if (pressed) {
                if (held == client.heldKeysyms.end()) {
                    if (client.heldKeysyms.size() >= kMaxHeldKeys) {
                        protocolFailure(client, "RFB client exceeded the bounded held-key limit");
                        return false;
                    }
                    publishTransition = holderCount == 0U;
                    client.heldKeysyms.push_back(keysym);
                    ++holderCount;
                } else {
                    // Preserve same-viewer repeated key-down behavior. A second viewer becoming an
                    // additional holder is not a new physical transition on the shared Qt target.
                    publishTransition = true;
                }
            } else if (held != client.heldKeysyms.end()) {
                client.heldKeysyms.erase(held);
                if (holderCount != 0U)
                    --holderCount;
                publishTransition = holderCount == 0U;
            } else {
                // An unmatched release from one viewer must never release another viewer's hold.
                publishTransition = false;
            }
        }

        client.modifiers = modifiersForHeldKeysyms(client.heldKeysyms);
        const hyremote::InputModifiers modifiers = aggregateModifiers();

        if (publishTransition) {
            hyremote::InputEvent event;
            event.kind = hyremote::InputEventKind::Key;
            event.key = key;
            event.pressed = pressed;
            event.modifiers = modifiers;
            publishInput(event);
        }

        if (!pressed || !publishTransition
            || hyremote::hasModifier(modifiers, hyremote::InputModifier::Control)
            || hyremote::hasModifier(modifiers, hyremote::InputModifier::Alt)
            || hyremote::hasModifier(modifiers, hyremote::InputModifier::Meta)) {
            return true;
        }

        std::string text = utf8ForKeysym(keysym);
        if (!text.empty()) {
            hyremote::InputEvent textEvent;
            textEvent.kind = hyremote::InputEventKind::Text;
            textEvent.modifiers = modifiers;
            textEvent.textUtf8 = std::move(text);
            publishInput(textEvent);
        }
        return true;
    }

    hyremote::InputViewport currentViewport() const
    {
        hyremote::InputViewport viewport;
        if (const auto frame = latestFrame()) {
            viewport.width = frame->geometry.size.width;
            viewport.height = frame->geometry.size.height;
        }
        viewport.devicePixelRatio = 1.0F;
        return viewport;
    }

    void deliverPointer(ClientState &client,
                        std::uint8_t mask,
                        std::uint16_t x,
                        std::uint16_t y)
    {
        const hyremote::InputViewport viewport = currentViewport();
        if (!hyremote::isValidInputViewport(viewport))
            return;

        client.lastPointerViewport = viewport;
        client.lastPointerX = x;
        client.lastPointerY = y;
        client.pointerPositionKnown = true;
        const hyremote::InputModifiers modifiers = aggregateModifiers();

        hyremote::InputEvent move;
        move.kind = hyremote::InputEventKind::PointerMove;
        move.sourceViewport = viewport;
        move.x = static_cast<float>(x);
        move.y = static_cast<float>(y);
        move.modifiers = modifiers;
        publishInput(move);

        for (std::size_t index = 0; index < kButtons.size(); ++index) {
            const ButtonBit &entry = kButtons[index];
            const bool before = (client.buttonMask & entry.bit) != 0;
            const bool after = (mask & entry.bit) != 0;
            if (before == after)
                continue;

            bool publishTransition = false;
            if (after) {
                publishTransition = m_buttonHolderCounts[index] == 0U;
                ++m_buttonHolderCounts[index];
            } else if (m_buttonHolderCounts[index] != 0U) {
                --m_buttonHolderCounts[index];
                publishTransition = m_buttonHolderCounts[index] == 0U;
            }
            if (!publishTransition)
                continue;

            hyremote::InputEvent button;
            button.kind = hyremote::InputEventKind::PointerButton;
            button.sourceViewport = viewport;
            button.x = static_cast<float>(x);
            button.y = static_cast<float>(y);
            button.button = entry.button;
            button.pressed = after;
            button.modifiers = modifiers;
            publishInput(button);
        }

        const std::uint8_t rising = static_cast<std::uint8_t>(mask & ~client.buttonMask);
        if ((rising & 0x78U) != 0) {
            hyremote::InputEvent scroll;
            scroll.kind = hyremote::InputEventKind::PointerScroll;
            scroll.sourceViewport = viewport;
            scroll.x = static_cast<float>(x);
            scroll.y = static_cast<float>(y);
            scroll.modifiers = modifiers;
            if (rising & 0x08U)
                scroll.scrollY += 1.0F;
            if (rising & 0x10U)
                scroll.scrollY -= 1.0F;
            if (rising & 0x20U)
                scroll.scrollX -= 1.0F;
            if (rising & 0x40U)
                scroll.scrollX += 1.0F;
            publishInput(scroll);
        }

        client.buttonMask = mask;
    }

    bool appendRawRectangle(QByteArray &message,
                            const hyremote::RemoteFrame &frame,
                            const PixelSpec &pixels,
                            std::uint16_t x,
                            std::uint16_t y,
                            std::uint16_t width,
                            std::uint16_t height)
    {
        if (frame.geometry.pixelFormat != hyremote::PixelFormat::Rgba8888 || !frame.storage)
            return false;
        const auto plane = frame.storage->mapRead(0);
        if (!plane || !plane->data)
            return false;
        const std::size_t frameWidth = frame.geometry.size.width;
        const std::size_t frameHeight = frame.geometry.size.height;
        if (plane->stride < frameWidth * 4U || plane->bytes < plane->stride * frameHeight)
            return false;

        appendU16(message, x);
        appendU16(message, y);
        appendU16(message, width);
        appendU16(message, height);
        appendS32(message, kEncodingRaw);

        const int bytesPerPixel = pixels.bitsPerPixel / 8;
        const std::uint64_t pixelBytes = static_cast<std::uint64_t>(width) * height * bytesPerPixel;
        if (pixelBytes > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            return false;
        message.reserve(message.size() + static_cast<qsizetype>(pixelBytes));

        const auto *base = reinterpret_cast<const std::uint8_t *>(plane->data);
        for (std::uint32_t row = 0; row < height; ++row) {
            const std::uint8_t *source = base + (static_cast<std::size_t>(y) + row) * plane->stride
                                         + static_cast<std::size_t>(x) * 4U;
            for (std::uint32_t col = 0; col < width; ++col) {
                appendEncodedPixel(message,
                                   source[col * 4U],
                                   source[col * 4U + 1U],
                                   source[col * 4U + 2U],
                                   pixels);
            }
        }
        return true;
    }

    void trySendUpdate(ClientState &client)
    {
        if (!client.socket || client.phase != ClientPhase::Normal || !client.updateRequested)
            return;
        // QTcpSocket's write buffer is itself a queue. Never append another framebuffer while the
        // previous one still has pending bytes; update requests remain a single coalesced flag.
        if (client.socket->bytesToWrite() != 0)
            return;

        const auto frame = latestFrame();
        if (!frame)
            return;
        if (frame->geometry.size.width == 0 || frame->geometry.size.height == 0
            || frame->geometry.size.width > std::numeric_limits<std::uint16_t>::max()
            || frame->geometry.size.height > std::numeric_limits<std::uint16_t>::max()) {
            protocolFailure(client, "RFB framebuffer geometry exceeds the protocol baseline");
            return;
        }

        const auto width = static_cast<std::uint16_t>(frame->geometry.size.width);
        const auto height = static_cast<std::uint16_t>(frame->geometry.size.height);
        const bool resized = width != client.framebufferWidth || height != client.framebufferHeight;
        if (resized && !client.supportsDesktopSize) {
            protocolFailure(client,
                            "RFB viewer does not advertise DesktopSize; reconnect after target resize");
            return;
        }

        std::uint16_t x = client.requestX;
        std::uint16_t y = client.requestY;
        std::uint16_t rectWidth = client.requestWidth;
        std::uint16_t rectHeight = client.requestHeight;
        if (resized) {
            x = 0;
            y = 0;
            rectWidth = width;
            rectHeight = height;
        } else {
            if (x >= width || y >= height) {
                client.updateRequested = false;
                return;
            }
            rectWidth = static_cast<std::uint16_t>(
                std::min<std::uint32_t>(rectWidth, static_cast<std::uint32_t>(width - x)));
            rectHeight = static_cast<std::uint16_t>(
                std::min<std::uint32_t>(rectHeight, static_cast<std::uint32_t>(height - y)));
        }
        if (rectWidth == 0 || rectHeight == 0) {
            client.updateRequested = false;
            return;
        }

        QByteArray update;
        update.append(char(0));  // FramebufferUpdate
        update.append(char(0));
        appendU16(update, resized ? 2 : 1);
        if (resized) {
            appendU16(update, 0);
            appendU16(update, 0);
            appendU16(update, width);
            appendU16(update, height);
            appendS32(update, kEncodingDesktopSize);
        }
        if (!appendRawRectangle(update, *frame, client.pixels, x, y, rectWidth, rectHeight)) {
            protocolFailure(client, "RFB transport could not map/encode the current RemoteFrame");
            return;
        }

        if (client.socket->write(update) < 0) {
            protocolFailure(client, "RFB socket write failed");
            return;
        }
        client.updateRequested = false;
        client.framebufferWidth = width;
        client.framebufferHeight = height;
    }

    QHostAddress m_address;
    quint16 m_port = 0;
    RfbSecurityConfig m_security;
    std::shared_ptr<SharedFrameState> m_frames;
    hyremote::InputHandler m_onInput;
    hyremote::TransportEventHandler m_onEvent;
    QTcpServer *m_server = nullptr;
    std::unordered_map<QTcpSocket *, std::unique_ptr<ClientState>> m_clients;
    std::array<std::size_t, kKeyCodeCount> m_keyHolderCounts{};
    std::array<std::size_t, kButtons.size()> m_buttonHolderCounts{};
    bool m_stopping = false;
};

class RfbTransport final : public hyremote::Transport
{
public:
    RfbTransport(QHostAddress address, quint16 port, RfbSecurityConfig security)
        : m_address(std::move(address))
        , m_port(port)
        , m_security(std::move(security))
        , m_frames(std::make_shared<SharedFrameState>())
    {
    }

    ~RfbTransport() override { stop(); }

    hyremote::FrameConsumerCapabilities frameCapabilities() const override
    {
        hyremote::FrameConsumerCapabilities result;
        result.acceptsCpu = true;
        result.cpuFormats = {hyremote::PixelFormat::Rgba8888};
        return result;
    }

    bool start(hyremote::InputHandler onInput, hyremote::TransportEventHandler onEvent) override
    {
        std::lock_guard<std::mutex> lock(m_lifecycleMutex);
        if (m_thread || m_worker || m_port == 0 || m_address.isNull())
            return false;

        auto thread = std::make_unique<QThread>();
        auto *worker = new RfbWorker(m_address, m_port, m_security, m_frames, std::move(onInput), std::move(onEvent));
        worker->moveToThread(thread.get());
        QObject::connect(thread.get(), &QThread::finished, worker, &QObject::deleteLater);
        thread->start();

        bool listening = false;
        const bool invoked = QMetaObject::invokeMethod(
            worker, [&listening, worker] { listening = worker->startServer(); }, Qt::BlockingQueuedConnection);
        if (!invoked || !listening) {
            QMetaObject::invokeMethod(worker, [worker] { worker->shutdown(); }, Qt::BlockingQueuedConnection);
            thread->quit();
            thread->wait();
            return false;
        }

        m_worker = worker;
        m_thread = std::move(thread);
        return true;
    }

    void stop() noexcept override
    {
        try {
            std::lock_guard<std::mutex> lock(m_lifecycleMutex);
            if (!m_thread || !m_worker)
                return;

            RfbWorker *worker = m_worker;
            QMetaObject::invokeMethod(worker, [worker] { worker->shutdown(); }, Qt::BlockingQueuedConnection);
            m_thread->quit();
            m_thread->wait();
            m_worker = nullptr;
            m_thread.reset();

            std::lock_guard<std::mutex> frameLock(m_frames->mutex);
            m_frames->latest.reset();
            m_frames->notificationPending = false;
        } catch (...) {
            // Transport::stop() is noexcept. Qt teardown above is designed not to throw; retain the
            // Core contract even if a platform allocation/standard-library edge case occurs.
        }
    }

    void enqueueFrame(hyremote::RemoteFrame frame) override
    {
        std::lock_guard<std::mutex> lifecycleLock(m_lifecycleMutex);
        if (!m_worker || !m_thread)
            return;

        bool notify = false;
        {
            std::lock_guard<std::mutex> frameLock(m_frames->mutex);
            m_frames->latest = std::move(frame);  // latest-frame-wins; capacity exactly one
            if (!m_frames->notificationPending) {
                m_frames->notificationPending = true;
                notify = true;
            }
        }
        if (notify) {
            RfbWorker *worker = m_worker;
            QMetaObject::invokeMethod(worker, [worker] { worker->frameAvailable(); }, Qt::QueuedConnection);
        }
    }

private:
    QHostAddress m_address;
    quint16 m_port = 0;
    RfbSecurityConfig m_security;
    std::shared_ptr<SharedFrameState> m_frames;
    std::mutex m_lifecycleMutex;
    std::unique_ptr<QThread> m_thread;
    RfbWorker *m_worker = nullptr;
};

}  // namespace

std::unique_ptr<hyremote::Transport> createRfbTransport(const QHostAddress &listenAddress,
                                                        quint16 port,
                                                        const RfbSecurityConfig &security)
{
    if (listenAddress.isNull() || port == 0)
        return {};

    // Defence in depth: the encrypted profile either carries the certificate and key its pre-listen preparation
    // validated, or it does not start. Nothing can fall through to a weaker security type.
    if (security.profile == RfbSecurityProfile::VeNCryptTlsVncAuth
        && (security.certificate.isNull() || security.privateKey.isNull())) {
        return {};
    }

    return std::make_unique<RfbTransport>(listenAddress, port, security);
}

}  // namespace HyRemote::detail