#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QPoint>
#include <QSize>
#include <QTcpSocket>
#include <QTimer>

class RfbClient final : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        Connecting,
        Handshaking,
        Connected,
        Failed,
    };
    Q_ENUM(State)

    struct Stats {
        double fps = 0.0;
        double mbitPerSecond = 0.0;
        quint64 frames = 0;
        quint64 bytes = 0;
        QSize framebufferSize;
    };

    explicit RfbClient(QObject *parent = nullptr);
    ~RfbClient() override;

    void connectToHost(const QString &host, quint16 port);
    void disconnectFromHost();

    State state() const { return m_state; }
    QString lastError() const { return m_error; }
    QString serverName() const { return m_serverName; }
    QSize framebufferSize() const { return m_size; }
    Stats stats() const { return m_stats; }
    bool remoteControlEnabled() const { return m_remoteControl; }

    void setRemoteControlEnabled(bool enabled) { m_remoteControl = enabled; }
    void sendPointer(quint8 buttonMask, quint16 x, quint16 y);
    void sendKey(quint32 keysym, bool pressed);

signals:
    void stateChanged(RfbClient::State state);
    void frameUpdated(const QImage &frame);
    void cursorUpdated(const QImage &cursor, const QPoint &hotspot);
    void serverNameChanged(const QString &name);
    void statsUpdated(const RfbClient::Stats &stats);
    void errorOccurred(const QString &message);

private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    enum class Handshake {
        Version,
        SecurityTypes,
        SecurityResult,
        ServerInit,
        Streaming,
    };

    void process();
    void setState(State state);
    void fail(const QString &message);
    void resetSession();
    void sendClientInit();
    void sendSetEncodings();
    void requestFrame(bool incremental);
    void writeMessage(const QByteArray &message);
    void pollStats();
    bool parseFramebufferUpdate();

    QTcpSocket m_socket;
    QByteArray m_in;
    QTimer m_statsPoll;
    QElapsedTimer m_statsClock;

    State m_state = State::Disconnected;
    Handshake m_handshake = Handshake::Version;
    QString m_error;
    QString m_serverName;
    QImage m_frame;
    QSize m_size;
    bool m_remoteControl = false;
    bool m_waitingForFrame = false;

    quint64 m_frames = 0;
    quint64 m_bytes = 0;
    quint64 m_lastFrames = 0;
    quint64 m_lastBytes = 0;
    Stats m_stats;
};
