#pragma once

#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QSize>
#include <QTcpSocket>

#include <cstdint>

namespace HyRemote::Qpa::Test {

constexpr std::int32_t kEncodingRaw = 0;
constexpr std::int32_t kEncodingDesktopSize = -223;

inline std::uint8_t byteAt(const QByteArray &data, qsizetype offset)
{
    return static_cast<std::uint8_t>(static_cast<unsigned char>(data.at(offset)));
}

inline std::uint16_t readU16(const QByteArray &data, qsizetype offset)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(byteAt(data, offset)) << 8U)
                                      | byteAt(data, offset + 1));
}

inline std::uint32_t readU32(const QByteArray &data, qsizetype offset)
{
    return (static_cast<std::uint32_t>(byteAt(data, offset)) << 24U)
           | (static_cast<std::uint32_t>(byteAt(data, offset + 1)) << 16U)
           | (static_cast<std::uint32_t>(byteAt(data, offset + 2)) << 8U)
           | static_cast<std::uint32_t>(byteAt(data, offset + 3));
}

inline std::int32_t readS32(const QByteArray &data, qsizetype offset)
{
    return static_cast<std::int32_t>(readU32(data, offset));
}

inline void appendU16(QByteArray &data, std::uint16_t value)
{
    data.append(static_cast<char>((value >> 8U) & 0xffU));
    data.append(static_cast<char>(value & 0xffU));
}

inline void appendU32(QByteArray &data, std::uint32_t value)
{
    data.append(static_cast<char>((value >> 24U) & 0xffU));
    data.append(static_cast<char>((value >> 16U) & 0xffU));
    data.append(static_cast<char>((value >> 8U) & 0xffU));
    data.append(static_cast<char>(value & 0xffU));
}

inline void appendS32(QByteArray &data, std::int32_t value)
{
    appendU32(data, static_cast<std::uint32_t>(value));
}

inline void pumpEvents(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

struct FramebufferGeometry
{
    std::uint16_t width = 0;
    std::uint16_t height = 0;

    QSize size() const { return QSize(width, height); }
};

class RfbTestClient
{
public:
    explicit RfbTestClient(quint16 port)
        : m_port(port)
    {
    }

    bool connectAndHandshake(int timeoutMs = 5000)
    {
        m_socket.connectToHost(QHostAddress::LocalHost, m_port);
        if (!waitForConnected(timeoutMs))
            return false;

        const QByteArray serverVersion = readExact(12, timeoutMs);
        if (serverVersion != QByteArray("RFB 003.008\n", 12))
            return false;
        if (!writeMessage(QByteArray("RFB 003.008\n", 12)))
            return false;

        const QByteArray securityTypes = readExact(2, timeoutMs);
        if (securityTypes.size() != 2 || byteAt(securityTypes, 0) != 1
            || byteAt(securityTypes, 1) != 1) {
            return false;
        }
        if (!writeMessage(QByteArray(1, char(1))))
            return false;

        const QByteArray securityResult = readExact(4, timeoutMs);
        if (securityResult.size() != 4 || readU32(securityResult, 0) != 0)
            return false;

        if (!writeMessage(QByteArray(1, char(1))))  // ClientInit: shared
            return false;

        const QByteArray init = readExact(24, timeoutMs);
        if (init.size() != 24)
            return false;
        m_geometry.width = readU16(init, 0);
        m_geometry.height = readU16(init, 2);
        const std::uint32_t nameLength = readU32(init, 20);
        if (m_geometry.width == 0 || m_geometry.height == 0 || nameLength > 1024)
            return false;
        if (readExact(static_cast<qsizetype>(nameLength), timeoutMs).size()
            != static_cast<qsizetype>(nameLength)) {
            return false;
        }

        QByteArray encodings;
        encodings.append(char(2));
        encodings.append(char(0));
        appendU16(encodings, 2);
        appendS32(encodings, kEncodingRaw);
        appendS32(encodings, kEncodingDesktopSize);
        return writeMessage(encodings);
    }

    bool requestFramebuffer(int timeoutMs = 5000)
    {
        if (!connected() || m_geometry.width == 0 || m_geometry.height == 0)
            return false;

        QByteArray request;
        request.append(char(3));
        request.append(char(0));  // full update
        appendU16(request, 0);
        appendU16(request, 0);
        appendU16(request, m_geometry.width);
        appendU16(request, m_geometry.height);
        if (!writeMessage(request))
            return false;
        return readFramebufferUpdate(timeoutMs);
    }

    bool waitForGeometry(const QSize &expected, int timeoutMs = 5000)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            pumpEvents(80);
            if (!requestFramebuffer(qMin(1500, timeoutMs - static_cast<int>(timer.elapsed()))))
                return false;
            if (m_geometry.size() == expected)
                return true;
        }
        return false;
    }

    bool connected() const { return m_socket.state() == QAbstractSocket::ConnectedState; }
    FramebufferGeometry geometry() const { return m_geometry; }
    QTcpSocket &socket() { return m_socket; }

private:
    bool waitForConnected(int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        while (!connected() && timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            m_socket.waitForConnected(20);
        }
        return connected();
    }

    QByteArray readExact(qsizetype count, int timeoutMs)
    {
        QByteArray result;
        QElapsedTimer timer;
        timer.start();
        while (result.size() < count && timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            const qsizetype needed = count - result.size();
            if (m_socket.bytesAvailable() > 0)
                result += m_socket.read(needed);
            else
                m_socket.waitForReadyRead(20);
            if (m_socket.state() == QAbstractSocket::UnconnectedState
                && m_socket.bytesAvailable() == 0) {
                break;
            }
        }
        return result;
    }

    bool writeMessage(const QByteArray &message)
    {
        if (!connected())
            return false;
        if (m_socket.write(message) != message.size())
            return false;
        m_socket.flush();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        return connected();
    }

    bool readFramebufferUpdate(int timeoutMs)
    {
        const QByteArray header = readExact(4, timeoutMs);
        if (header.size() != 4 || byteAt(header, 0) != 0)
            return false;

        const std::uint16_t rectangleCount = readU16(header, 2);
        if (rectangleCount == 0 || rectangleCount > 4)
            return false;

        FramebufferGeometry announced = m_geometry;
        for (std::uint16_t i = 0; i < rectangleCount; ++i) {
            const QByteArray rect = readExact(12, timeoutMs);
            if (rect.size() != 12)
                return false;
            const std::uint16_t width = readU16(rect, 4);
            const std::uint16_t height = readU16(rect, 6);
            const std::int32_t encoding = readS32(rect, 8);

            if (encoding == kEncodingDesktopSize) {
                announced.width = width;
                announced.height = height;
                continue;
            }
            if (encoding != kEncodingRaw)
                return false;

            const std::uint64_t bytes = static_cast<std::uint64_t>(width) * height * 4U;
            if (bytes > 16U * 1024U * 1024U)
                return false;
            if (readExact(static_cast<qsizetype>(bytes), timeoutMs).size()
                != static_cast<qsizetype>(bytes)) {
                return false;
            }
        }

        m_geometry = announced;
        return connected();
    }

    quint16 m_port = 0;
    QTcpSocket m_socket;
    FramebufferGeometry m_geometry;
};

}  // namespace HyRemote::Qpa::Test
