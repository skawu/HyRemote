#include "framebuffer_view.h"
#include "rfb_client.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <iostream>

namespace {

QString humanBytes(quint64 bytes)
{
    constexpr double kKiB = 1024.0;
    constexpr double kMiB = kKiB * 1024.0;
    constexpr double kGiB = kMiB * 1024.0;
    if (bytes >= static_cast<quint64>(kGiB))
        return QStringLiteral("%1 GiB").arg(bytes / kGiB, 0, 'f', 2);
    if (bytes >= static_cast<quint64>(kMiB))
        return QStringLiteral("%1 MiB").arg(bytes / kMiB, 0, 'f', 1);
    if (bytes >= static_cast<quint64>(kKiB))
        return QStringLiteral("%1 KiB").arg(bytes / kKiB, 0, 'f', 1);
    return QStringLiteral("%1 B").arg(bytes);
}

QString stateText(RfbClient::State state)
{
    switch (state) {
    case RfbClient::State::Disconnected:
        return QStringLiteral("Disconnected");
    case RfbClient::State::Connecting:
        return QStringLiteral("Connecting");
    case RfbClient::State::Handshaking:
        return QStringLiteral("Handshaking");
    case RfbClient::State::Connected:
        return QStringLiteral("Connected");
    case RfbClient::State::Failed:
        return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}

class ClientWindow final : public QWidget
{
public:
    explicit ClientWindow(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_client(new RfbClient(this))
        , m_view(new FramebufferView(this))
    {
        setWindowTitle(QStringLiteral("HyRemoteTool — Remote access client"));

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(10, 10, 10, 10);
        root->setSpacing(8);

        auto *connectionRow = new QHBoxLayout;
        auto *remoteLabel = new QLabel(QStringLiteral("Remote"), this);
        m_host = new QLineEdit(this);
        m_host->setPlaceholderText(QStringLiteral("192.168.1.6"));
        m_host->setClearButtonEnabled(true);
        auto *portLabel = new QLabel(QStringLiteral("Port"), this);
        m_port = new QSpinBox(this);
        m_port->setRange(1, 65535);
        m_port->setValue(5921);
        m_connect = new QPushButton(QStringLiteral("Connect"), this);
        m_status = new QLabel(QStringLiteral("Disconnected"), this);
        connectionRow->addWidget(remoteLabel);
        connectionRow->addWidget(m_host, 1);
        connectionRow->addWidget(portLabel);
        connectionRow->addWidget(m_port);
        connectionRow->addWidget(m_connect);
        connectionRow->addWidget(m_status);
        root->addLayout(connectionRow);

        root->addWidget(m_view, 1);

        auto *telemetryRow = new QHBoxLayout;
        m_control = new QCheckBox(QStringLiteral("Remote control"), this);
        m_control->setToolTip(QStringLiteral(
            "When unchecked the session is view-only and no pointer or keyboard events are sent."));
        m_fps = new QLabel(QStringLiteral("FPS 0.0"), this);
        m_mbps = new QLabel(QStringLiteral("0.00 Mbit/s"), this);
        m_frames = new QLabel(QStringLiteral("Frames 0"), this);
        m_bytes = new QLabel(QStringLiteral("Bytes 0 B"), this);
        m_resolution = new QLabel(QStringLiteral("No frame"), this);
        telemetryRow->addWidget(m_control);
        telemetryRow->addSpacing(16);
        telemetryRow->addWidget(m_fps);
        telemetryRow->addWidget(m_mbps);
        telemetryRow->addWidget(m_frames);
        telemetryRow->addWidget(m_bytes);
        telemetryRow->addWidget(m_resolution);
        telemetryRow->addStretch(1);
        root->addLayout(telemetryRow);

        m_error = new QLabel(this);
        m_error->setWordWrap(true);
        m_error->setStyleSheet(QStringLiteral("color: #b00020;"));
        m_error->setVisible(false);
        root->addWidget(m_error);

        m_view->onPointer = [this](quint8 mask, quint16 x, quint16 y) {
            m_client->sendPointer(mask, x, y);
        };
        m_view->onKey = [this](quint32 keysym, bool pressed) {
            m_client->sendKey(keysym, pressed);
        };

        connect(m_connect, &QPushButton::clicked, this, [this] { toggleConnection(); });
        connect(m_control, &QCheckBox::toggled, this, [this](bool enabled) {
            m_client->setRemoteControlEnabled(enabled);
            m_view->setInputEnabled(enabled);
        });
        connect(m_client, &RfbClient::stateChanged, this, [this](RfbClient::State state) {
            applyState(state);
        });
        connect(m_client, &RfbClient::frameUpdated, this, [this](const QImage &frame) {
            m_view->setFrame(frame);
            m_resolution->setText(QStringLiteral("%1 × %2").arg(frame.width()).arg(frame.height()));
            std::cout << "TOOL_CLIENT_FRAME " << frame.width() << 'x' << frame.height() << std::endl;
        });
        connect(m_client, &RfbClient::statsUpdated, this, [this](const RfbClient::Stats &stats) {
            m_fps->setText(QStringLiteral("FPS %1").arg(stats.fps, 0, 'f', 1));
            m_mbps->setText(QStringLiteral("%1 Mbit/s").arg(stats.mbitPerSecond, 0, 'f', 2));
            m_frames->setText(QStringLiteral("Frames %1").arg(stats.frames));
            m_bytes->setText(QStringLiteral("Bytes %1").arg(humanBytes(stats.bytes)));
            std::cout << "TOOL_CLIENT_STATS fps=" << stats.fps
                      << " mbit=" << stats.mbitPerSecond
                      << " frames=" << stats.frames
                      << " bytes=" << stats.bytes << std::endl;
        });
        connect(m_client, &RfbClient::errorOccurred, this, [this](const QString &message) {
            m_error->setText(message);
            m_error->setVisible(true);
            std::cerr << "TOOL_CLIENT_ERROR " << message.toStdString() << std::endl;
        });

        m_view->clearFrame(QStringLiteral(
            "Not connected. Enter the remote application's IP and port, then Connect."));
        m_view->setInputEnabled(false);
        m_client->setRemoteControlEnabled(false);
        applyState(RfbClient::State::Disconnected);
        resize(1000, 700);
    }

    void connectTo(const QString &host, quint16 port)
    {
        m_host->setText(host);
        m_port->setValue(port);
        m_client->connectToHost(host, port);
    }

    bool verifyUi() const
    {
        const bool ok = m_host && m_port && m_connect && m_view && m_status
                        && m_control && m_fps && m_mbps && m_frames && m_bytes
                        && m_resolution && m_error;
        std::cout << (ok ? "TOOL_CLIENT_UI_PASS" : "TOOL_CLIENT_UI_FAIL")
                  << " host=" << (m_host != nullptr)
                  << " port=" << (m_port != nullptr)
                  << " view=" << (m_view != nullptr)
                  << " telemetry=" << (m_fps != nullptr && m_mbps != nullptr)
                  << std::endl;
        return ok;
    }

protected:
    void closeEvent(QCloseEvent *event) override
    {
        m_client->disconnectFromHost();
        QWidget::closeEvent(event);
    }

private:
    void toggleConnection()
    {
        const auto state = m_client->state();
        if (state == RfbClient::State::Connected || state == RfbClient::State::Connecting
            || state == RfbClient::State::Handshaking) {
            m_client->disconnectFromHost();
            return;
        }

        const QString host = m_host->text().trimmed();
        if (host.isEmpty()) {
            m_error->setText(QStringLiteral("Enter the remote application IP or hostname."));
            m_error->setVisible(true);
            return;
        }
        m_error->setVisible(false);
        m_client->connectToHost(host, static_cast<quint16>(m_port->value()));
    }

    void applyState(RfbClient::State state)
    {
        m_status->setText(stateText(state));
        std::cout << "TOOL_CLIENT_STATE " << stateText(state).toStdString() << std::endl;

        const bool busy = state == RfbClient::State::Connecting
                          || state == RfbClient::State::Handshaking;
        const bool connected = state == RfbClient::State::Connected;
        m_host->setEnabled(!busy && !connected);
        m_port->setEnabled(!busy && !connected);
        m_connect->setText(busy || connected ? QStringLiteral("Disconnect")
                                            : QStringLiteral("Connect"));
        m_connect->setEnabled(true);
        m_control->setEnabled(connected);
        if (!connected) {
            m_control->setChecked(false);
            m_view->setInputEnabled(false);
            m_client->setRemoteControlEnabled(false);
        }
        if (state == RfbClient::State::Disconnected) {
            m_view->clearFrame(QStringLiteral(
                "Not connected. Enter the remote application's IP and port, then Connect."));
            m_resolution->setText(QStringLiteral("No frame"));
            m_error->setVisible(false);
        }
        if (state == RfbClient::State::Failed) {
            m_view->clearFrame(QStringLiteral("Connection failed. Check the remote endpoint and try again."));
            m_resolution->setText(QStringLiteral("No frame"));
        }
    }

    RfbClient *m_client = nullptr;
    FramebufferView *m_view = nullptr;
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QPushButton *m_connect = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_error = nullptr;
    QLabel *m_fps = nullptr;
    QLabel *m_mbps = nullptr;
    QLabel *m_frames = nullptr;
    QLabel *m_bytes = nullptr;
    QLabel *m_resolution = nullptr;
    QCheckBox *m_control = nullptr;
};

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HyRemoteTool"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("HyRemoteTool — connect to a remote HyRemote-enabled application."));
    parser.addHelpOption();

    QCommandLineOption connectOption(
        QStringLiteral("connect"),
        QStringLiteral("Connect to <host>:<port> after startup."),
        QStringLiteral("host:port"));
    QCommandLineOption secondsOption(
        QStringLiteral("test-seconds"),
        QStringLiteral("Exit after N seconds (CI helper)."),
        QStringLiteral("seconds"),
        QStringLiteral("0"));
    QCommandLineOption verifyUiOption(
        QStringLiteral("verify-ui"),
        QStringLiteral("CI-only check: construct the client window without connecting."));
    parser.addOption(connectOption);
    parser.addOption(secondsOption);
    parser.addOption(verifyUiOption);
    parser.process(app);

    ClientWindow window;
    window.show();

    if (parser.isSet(verifyUiOption)) {
        QTimer::singleShot(0, &window, [&window, &app] {
            app.exit(window.verifyUi() ? 0 : 1);
        });
        return app.exec();
    }

    const QString connectTarget = parser.value(connectOption);
    if (!connectTarget.isEmpty()) {
        const int separator = connectTarget.lastIndexOf(QLatin1Char(':'));
        if (separator <= 0) {
            std::cerr << "invalid --connect; expected host:port" << std::endl;
            return 64;
        }
        bool ok = false;
        const int port = connectTarget.mid(separator + 1).toInt(&ok);
        if (!ok || port <= 0 || port > 65535) {
            std::cerr << "invalid --connect port" << std::endl;
            return 64;
        }
        const QString host = connectTarget.left(separator);
        QTimer::singleShot(0, &window, [&window, host, port] {
            window.connectTo(host, static_cast<quint16>(port));
        });
    }

    const int testSeconds = parser.value(secondsOption).toInt();
    if (testSeconds > 0)
        QTimer::singleShot(testSeconds * 1000, &app, &QCoreApplication::quit);

    return app.exec();
}
