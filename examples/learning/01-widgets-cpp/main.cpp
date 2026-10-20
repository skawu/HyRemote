// HyRemote V0.1 learning example 01: the smallest real Embedded C++ path around a Widgets window.
//
// Read these four lines first - they are the whole integration:
//
//     HyRemote::RemoteAccess remote(&window);
//     remote.setPort(5921);
//     remote.start();
//     remote.stop();
//
// Everything else here exists so the example can be launched by a person (a visible window, a status line) and by
// CI (an explicit port, a bounded run, a clean shutdown), not to demonstrate HyRemote internals. The example never
// touches Core, Session, capture, input, transport or target-adapter types: those are runtime composition details,
// and application code is not supposed to know them.

#include <HyRemote/RemoteAccess.h>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMouseEvent>
#include <QTimer>
#include <QVBoxLayout>

#include <iostream>

namespace {

// The example is also the manual validation target. Echo accepted input so an
// external viewer run can prove that keyboard/pointer events reached Qt without
// relying only on a visual inspection of the text field.
class ValidationLineEdit final : public QLineEdit
{
public:
    using QLineEdit::QLineEdit;

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        std::cout << "INPUT_KEY press key=" << event->key()
                  << " text=" << event->text().toStdString() << std::endl;
        QLineEdit::keyPressEvent(event);
    }

    void keyReleaseEvent(QKeyEvent *event) override
    {
        std::cout << "INPUT_KEY release key=" << event->key() << std::endl;
        QLineEdit::keyReleaseEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        std::cout << "INPUT_MOUSE press button=" << static_cast<int>(event->button())
                  << " x=" << event->position().x()
                  << " y=" << event->position().y() << std::endl;
        QLineEdit::mousePressEvent(event);
    }
};

int positiveInt(const QCommandLineParser &parser, const QCommandLineOption &option, int fallback)
{
    bool ok = false;
    const int value = parser.value(option).toInt(&ok);
    return ok && value > 0 ? value : fallback;
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HyRemote Learning 01 - Widgets + C++"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("HyRemote Embedded C++ API over a QMainWindow"));
    parser.addHelpOption();
    QCommandLineOption portOption(QStringList{QStringLiteral("p"), QStringLiteral("port")},
                                  QStringLiteral("Listener port the viewer connects to."),
                                  QStringLiteral("port"),
                                  QStringLiteral("5921"));
    QCommandLineOption inputOption(
        QStringLiteral("remote-input"),
        QStringLiteral("Enable remote keyboard/pointer input. Without it the application is view-only."));
    QCommandLineOption secondsOption(QStringLiteral("test-seconds"),
                                     QStringLiteral("Exit by itself after N seconds (used by the smoke test)."),
                                     QStringLiteral("seconds"),
                                     QStringLiteral("0"));
    parser.addOption(portOption);
    parser.addOption(inputOption);
    parser.addOption(secondsOption);
    parser.process(app);

    const int port = positiveInt(parser, portOption, 5921);
    if (port > 65535) {
        std::cerr << "invalid port: " << port << std::endl;
        return 64;
    }
    const int testSeconds = positiveInt(parser, secondsOption, 0);
    const bool remoteInput = parser.isSet(inputOption);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("HyRemote Learning 01 - Widgets + C++"));
    window.resize(480, 240);

    auto *central = new QWidget(&window);
    auto *layout = new QVBoxLayout(central);
    auto *status = new QLabel(central);
    status->setWordWrap(true);
    status->setText(QStringLiteral("remote control: %1\nlistener: 0.0.0.0:%2\nconnect a VNC viewer to this host's "
                                   "LAN IPv4 address on port %2")
                        .arg(remoteInput ? QStringLiteral("enabled explicitly") : QStringLiteral("view-only (default)"))
                        .arg(port));
    auto *editor = new ValidationLineEdit(central);
    editor->setPlaceholderText(QStringLiteral("Type here to test remote keyboard input"));
    editor->setCursor(Qt::IBeamCursor);
    layout->addWidget(status);
    layout->addWidget(editor);
    window.setCentralWidget(central);
    window.show();
    editor->setFocus();

    HyRemote::RemoteAccess remote(&window);
    remote.setPort(static_cast<quint16>(port));
    // Remote input stays off unless the user asks for it: the shipped listener is reachable on this host's IPv4
    // interfaces, which is why it is for a trusted LAN only and the reported security state says so.
    remote.setRemoteInputEnabled(remoteInput);
    if (!remote.start()) {
        const auto error = remote.lastError();
        std::cerr << "START_FAILED";
        if (error)
            std::cerr << " " << error->message.toStdString();
        std::cerr << std::endl;
        return 2;
    }

    std::cout << "APP_READY" << std::endl;
    std::cout << "PLATFORM_NAME=" << QGuiApplication::platformName().toStdString() << std::endl;
    if (remote.state() != HyRemote::RemoteAccessState::Running) {
        std::cerr << "unexpected state after start()" << std::endl;
        remote.stop();
        return 3;
    }
    std::cout << "HYREMOTE_START=Running" << std::endl;

    if (testSeconds > 0)
        QTimer::singleShot(testSeconds * 1000, &app, &QCoreApplication::quit);

    const int result = app.exec();

    remote.stop();
    if (remote.state() != HyRemote::RemoteAccessState::Stopped)
        std::cerr << "unexpected state after stop()" << std::endl;
    std::cout << "HYREMOTE_STOP=Stopped" << std::endl;
    return result;
}
