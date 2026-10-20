#include "rfb_client.h"

#include <QAbstractSocket>
#include <QStringList>
#include <QVector>

#include <cstring>

namespace {

constexpr qsizetype kMaxRawRectangleBytes = 64 * 1024 * 1024;
constexpr qint32 kEncodingRaw = 0;
constexpr qint32 kEncodingDesktopSize = -223;

quint16 readU16(const QByteArray &data, qsizetype offset)
{
    return static_cast<quint16>((static_cast<quint16>(static_cast<unsigned char>(data.at(offset)))
                                 << 8U)
                                | static_cast<unsigned char>(data.at(offset + 1)));
}

quint32 readU32(const QByteArray &data, qsizetype offset)
{
    return (static_cast<quint32>(static_cast<unsigned char>(data.at(offset))) << 24U)
           | (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 1))) << 16U)
           | (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 2))) << 8U)
           | static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 3)));
}

qint32 readS32(const QByteArray &data, qsizetype offset)
{
    return static_cast<qint32>(readU32(data, offset));
}

void appendU16(QByteArray &data, quint16 value)
{
    data.append(static_cast<char>((value >> 8U) & 0xffU));
    data.append(static_cast<char>(value & 0xffU));
}

void appendS32(QByteArray &data, qint32 value)
{
    const quint32 raw = static_cast<quint32>(value);
    data.append(static_cast<char>((raw >> 24U) & 0xffU));
    data.append(static_cast<char>((raw >> 16U) & 0xffU));
    data.append(static_cast<char>((raw >> 8U) & 0xffU));
    data.append(static_cast<char>(raw & 0xffU));
}

}  // namespace

RfbClient::RfbClient(QObject *parent)
    : QObject(parent)
{
    m_statsPoll.setInterval(500);
    m_statsPoll.setTimerType(Qt::CoarseTimer);
    connect(&m_statsPoll, &QTimer::timeout, this, &RfbClient::pollStats);

    connect(&m_socket, &QTcpSocket::connected, this, &RfbClient::onConnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &RfbClient::onReadyRead);
    connect(&m_socket, &QTcpSocket::disconnected, this, &RfbClient::onDisconnected);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &RfbClient::onSocketError);
}

RfbClient::~RfbClient()
{
    m_socket.abort();
}

void RfbClient::connectToHost(const QString &host, quint16 port)
{
    disconnectFromHost();
    resetSession();
    m_error.clear();
    m_handshake = Handshake::Version;
    setState(State::Connecting);
    m_socket.connectToHost(host, port);
}

void RfbClient::disconnectFromHost()
{
    m_statsPoll.stop();
    m_socket.abort();
    m_in.clear();
    m_waitingForFrame = false;
    setState(State::Disconnected);
}

void RfbClient::onConnected()
{
    m_statsClock.start();
    m_statsPoll.start();
    setState(State::Handshaking);
    // RFB servers speak first; process() answers with our preferred 3.8 after
    // the server version arrives. Sending it here as well would desynchronise
    // the handshake with a second version banner.
}

void RfbClient::onReadyRead()
{
    const qint64 available = m_socket.bytesAvailable();
    if (available <= 0)
        return;
    m_bytes += static_cast<quint64>(available);
    m_in += m_socket.readAll();
    process();
}

void RfbClient::onDisconnected()
{
    m_statsPoll.stop();
    m_in.clear();
    m_waitingForFrame = false;
    if (m_state != State::Failed)
        setState(State::Disconnected);
}

void RfbClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    if (m_state == State::Disconnected || m_state == State::Failed)
        return;
    fail(m_socket.errorString());
}

void RfbClient::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(m_state);
}

void RfbClient::fail(const QString &message)
{
    if (m_state == State::Failed)
        return;
    m_error = message;
    m_statsPoll.stop();
    m_socket.abort();
    m_in.clear();
    emit errorOccurred(m_error);
    setState(State::Failed);
}

void RfbClient::resetSession()
{
    m_in.clear();
    m_frame = QImage();
    m_size = QSize();
    m_serverName.clear();
    m_waitingForFrame = false;
    m_frames = 0;
    m_bytes = 0;
    m_lastFrames = 0;
    m_lastBytes = 0;
    m_stats = Stats{};
}

void RfbClient::writeMessage(const QByteArray &message)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState)
        return;
    if (m_socket.write(message) != message.size()) {
        fail(QStringLiteral("could not write to the remote endpoint"));
        return;
    }
    m_socket.flush();
}

void RfbClient::process()
{
    while (m_state != State::Disconnected && m_state != State::Failed) {
        if (m_handshake == Handshake::Version) {
            if (m_in.size() < 12)
                return;
            const QByteArray version = m_in.left(12);
            m_in.remove(0, 12);
            if (!version.startsWith("RFB 003.00") || version.at(11) != '\n') {
                fail(QStringLiteral("not an RFB 3.x endpoint"));
                return;
            }
            writeMessage(QByteArray("RFB 003.008\n", 12));
            m_handshake = Handshake::SecurityTypes;
            continue;
        }

        if (m_handshake == Handshake::SecurityTypes) {
            if (m_in.isEmpty())
                return;
            const int count = static_cast<unsigned char>(m_in.at(0));
            if (count == 0) {
                if (m_in.size() < 5)
                    return;
                const quint32 reasonLength = readU32(m_in, 1);
                if (reasonLength > 4096 || m_in.size() < 5 + static_cast<qsizetype>(reasonLength))
                    return;
                const QString reason = QString::fromUtf8(
                    m_in.mid(5, static_cast<qsizetype>(reasonLength)));
                m_in.remove(0, 5 + static_cast<qsizetype>(reasonLength));
                fail(reason.isEmpty() ? QStringLiteral("the remote endpoint refused the connection")
                                      : reason);
                return;
            }
            if (m_in.size() < 1 + count)
                return;
            const QByteArray types = m_in.mid(1, count);
            m_in.remove(0, 1 + count);
            bool offersNone = false;
            QStringList offered;
            for (char type : types) {
                const int value = static_cast<unsigned char>(type);
                offered.push_back(QString::number(value));
                if (value == 1)
                    offersNone = true;
            }
            if (!offersNone) {
                fail(QStringLiteral("the remote endpoint requires security type(s) %1; "
                                    "this version supports None only")
                         .arg(offered.join(QStringLiteral(", "))));
                return;
            }
            writeMessage(QByteArray(1, char(1)));
            m_handshake = Handshake::SecurityResult;
            continue;
        }

        if (m_handshake == Handshake::SecurityResult) {
            if (m_in.size() < 4)
                return;
            const quint32 result = readU32(m_in, 0);
            if (result != 0) {
                if (m_in.size() < 8)
                    return;
                const quint32 reasonLength = readU32(m_in, 4);
                if (reasonLength > 4096 || m_in.size() < 8 + static_cast<qsizetype>(reasonLength))
                    return;
                const QString reason = QString::fromUtf8(
                    m_in.mid(8, static_cast<qsizetype>(reasonLength)));
                m_in.remove(0, 8 + static_cast<qsizetype>(reasonLength));
                fail(reason.isEmpty() ? QStringLiteral("remote authentication failed") : reason);
                return;
            }
            m_in.remove(0, 4);
            // ClientInit: shared session.
            writeMessage(QByteArray(1, char(1)));
            m_handshake = Handshake::ServerInit;
            continue;
        }

        if (m_handshake == Handshake::ServerInit) {
            if (m_in.size() < 24)
                return;
            const int width = readU16(m_in, 0);
            const int height = readU16(m_in, 2);
            const quint32 nameLength = readU32(m_in, 20);
            if (width <= 0 || height <= 0) {
                fail(QStringLiteral("the remote endpoint announced an empty framebuffer"));
                return;
            }
            if (nameLength > 4096 || m_in.size() < 24 + static_cast<qsizetype>(nameLength))
                return;
            m_serverName = QString::fromUtf8(m_in.mid(24, static_cast<qsizetype>(nameLength)));
            m_in.remove(0, 24 + static_cast<qsizetype>(nameLength));
            m_size = QSize(width, height);
            m_frame = QImage(width, height, QImage::Format_RGBX8888);
            if (m_frame.isNull()) {
                fail(QStringLiteral("could not allocate the remote framebuffer"));
                return;
            }
            m_frame.fill(Qt::black);
            sendSetEncodings();
            m_handshake = Handshake::Streaming;
            setState(State::Connected);
            requestFrame(false);
            continue;
        }

        if (m_handshake == Handshake::Streaming) {
            if (m_in.isEmpty())
                return;
            const int type = static_cast<unsigned char>(m_in.at(0));
            if (type == 0) {
                if (!parseFramebufferUpdate())
                    return;
                continue;
            }
            if (type == 2) {  // Bell
                m_in.remove(0, 1);
                continue;
            }
            if (type == 3) {  // ServerCutText: ignored in this version
                if (m_in.size() < 8)
                    return;
                const quint32 length = readU32(m_in, 4);
                if (length > kMaxRawRectangleBytes
                    || m_in.size() < 8 + static_cast<qsizetype>(length))
                    return;
                m_in.remove(0, 8 + static_cast<qsizetype>(length));
                continue;
            }
            fail(QStringLiteral("unsupported server message type %1").arg(type));
            return;
        }
    }
}

void RfbClient::sendSetEncodings()
{
    QByteArray message;
    message.append(char(2));
    message.append(char(0));
    appendU16(message, 2);
    appendS32(message, kEncodingRaw);
    appendS32(message, kEncodingDesktopSize);
    writeMessage(message);
}

void RfbClient::requestFrame(bool incremental)
{
    if (m_state != State::Connected || m_waitingForFrame || m_size.isEmpty())
        return;
    QByteArray message;
    message.append(char(3));
    message.append(static_cast<char>(incremental ? 1 : 0));
    appendU16(message, 0);
    appendU16(message, 0);
    appendU16(message, static_cast<quint16>(m_size.width()));
    appendU16(message, static_cast<quint16>(m_size.height()));
    m_waitingForFrame = true;
    writeMessage(message);
}

bool RfbClient::parseFramebufferUpdate()
{
    if (m_in.size() < 4)
        return false;
    const quint16 rectangleCount = readU16(m_in, 2);
    if (rectangleCount > 64) {
        fail(QStringLiteral("remote framebuffer update has too many rectangles"));
        return false;
    }

    struct Rectangle {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        qint32 encoding = 0;
        qsizetype payloadOffset = 0;
        qsizetype payloadBytes = 0;
    };

    QVector<Rectangle> rectangles;
    rectangles.reserve(rectangleCount);
    qsizetype offset = 4;
    for (quint16 index = 0; index < rectangleCount; ++index) {
        if (m_in.size() < offset + 12)
            return false;
        Rectangle rectangle;
        rectangle.x = readU16(m_in, offset);
        rectangle.y = readU16(m_in, offset + 2);
        rectangle.width = readU16(m_in, offset + 4);
        rectangle.height = readU16(m_in, offset + 6);
        rectangle.encoding = readS32(m_in, offset + 8);
        rectangle.payloadOffset = offset + 12;

        if (rectangle.encoding == kEncodingRaw) {
            const qint64 bytes = static_cast<qint64>(rectangle.width) * rectangle.height * 4;
            if (bytes < 0 || bytes > kMaxRawRectangleBytes) {
                fail(QStringLiteral("remote raw rectangle is too large"));
                return false;
            }
            rectangle.payloadBytes = static_cast<qsizetype>(bytes);
        } else if (rectangle.encoding == kEncodingDesktopSize) {
            rectangle.payloadBytes = 0;
        } else {
            fail(QStringLiteral("unsupported rectangle encoding %1").arg(rectangle.encoding));
            return false;
        }

        if (m_in.size() < rectangle.payloadOffset + rectangle.payloadBytes)
            return false;
        offset = rectangle.payloadOffset + rectangle.payloadBytes;
        rectangles.push_back(rectangle);
    }

    bool fullRequestNeeded = false;
    for (const Rectangle &rectangle : rectangles) {
        if (rectangle.encoding == kEncodingDesktopSize) {
            if (rectangle.width <= 0 || rectangle.height <= 0) {
                fail(QStringLiteral("remote endpoint announced an empty framebuffer"));
                return false;
            }
            m_size = QSize(rectangle.width, rectangle.height);
            m_frame = QImage(m_size, QImage::Format_RGBX8888);
            if (m_frame.isNull()) {
                fail(QStringLiteral("could not allocate the remote framebuffer"));
                return false;
            }
            m_frame.fill(Qt::black);
            fullRequestNeeded = true;
            continue;
        }

        if (m_frame.isNull() || rectangle.x + rectangle.width > m_frame.width()
            || rectangle.y + rectangle.height > m_frame.height()) {
            fail(QStringLiteral("remote rectangle is outside the framebuffer"));
            return false;
        }

        const char *source = m_in.constData() + rectangle.payloadOffset;
        const qsizetype rowBytes = static_cast<qsizetype>(rectangle.width) * 4;
        for (int row = 0; row < rectangle.height; ++row) {
            std::memcpy(m_frame.scanLine(rectangle.y + row) + rectangle.x * 4,
                        source + row * rowBytes,
                        static_cast<std::size_t>(rowBytes));
        }
    }

    m_in.remove(0, offset);
    ++m_frames;
    m_waitingForFrame = false;
    if (!m_frame.isNull())
        emit frameUpdated(m_frame);
    requestFrame(!fullRequestNeeded);
    return true;
}

void RfbClient::sendPointer(quint8 buttonMask, quint16 x, quint16 y)
{
    if (m_state != State::Connected || !m_remoteControl || m_size.isEmpty())
        return;
    QByteArray message;
    message.append(char(5));
    message.append(static_cast<char>(buttonMask));
    appendU16(message, qMin<quint16>(x, static_cast<quint16>(m_size.width() - 1)));
    appendU16(message, qMin<quint16>(y, static_cast<quint16>(m_size.height() - 1)));
    writeMessage(message);
}

void RfbClient::sendKey(quint32 keysym, bool pressed)
{
    if (m_state != State::Connected || !m_remoteControl)
        return;
    QByteArray message;
    message.append(char(4));
    message.append(static_cast<char>(pressed ? 1 : 0));
    // RFB KeyEvent is 8 bytes: type, down-flag, two padding bytes, keysym.
    message.append("\0\0", 2);
    message.append(static_cast<char>((keysym >> 24U) & 0xffU));
    message.append(static_cast<char>((keysym >> 16U) & 0xffU));
    message.append(static_cast<char>((keysym >> 8U) & 0xffU));
    message.append(static_cast<char>(keysym & 0xffU));
    writeMessage(message);
}

void RfbClient::pollStats()
{
    const qint64 elapsedMs = m_statsClock.restart();
    if (elapsedMs <= 0)
        return;
    const double seconds = static_cast<double>(elapsedMs) / 1000.0;
    m_stats.fps = static_cast<double>(m_frames - m_lastFrames) / seconds;
    m_stats.mbitPerSecond = static_cast<double>(m_bytes - m_lastBytes) * 8.0 / seconds / 1e6;
    m_stats.frames = m_frames;
    m_stats.bytes = m_bytes;
    m_stats.framebufferSize = m_size;
    m_lastFrames = m_frames;
    m_lastBytes = m_bytes;
    emit statsUpdated(m_stats);
}
