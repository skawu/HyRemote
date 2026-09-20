#include <HyRemote/RemoteAccess.h>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

#include <iostream>

namespace {

class InputProbe final : public QObject
{
public:
    explicit InputProbe(QWidget *root)
        : m_root(root)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto *widget = qobject_cast<QWidget *>(watched);
        if (!widget || !m_root || (widget != m_root && !m_root->isAncestorOf(widget)))
            return false;

        // This probe is the acceptance instrument: one line must mean one delivery. Returning true for
        // the events it records stops Qt from propagating an accepted-but-unhandled event to the parent,
        // which would otherwise log the same key twice (the example's widgets are non-interactive, so
        // consuming does not change what the application does).
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            std::cout << "APP_POINTER button=" << static_cast<int>(mouse->button())
                      << " x=" << mouse->position().x()
                      << " y=" << mouse->position().y() << std::endl;
            break;
        }
        case QEvent::Wheel: {
            const auto *wheel = static_cast<QWheelEvent *>(event);
            std::cout << "APP_WHEEL y=" << wheel->angleDelta().y() << std::endl;
            break;
        }
        case QEvent::KeyPress: {
            const auto *key = static_cast<QKeyEvent *>(event);
            const Qt::KeyboardModifiers modifiers = key->modifiers();
            std::cout << "APP_KEY key=" << key->key()
                      << " shift=" << ((modifiers & Qt::ShiftModifier) ? 1 : 0)
                      << " ctrl=" << ((modifiers & Qt::ControlModifier) ? 1 : 0)
                      << " alt=" << ((modifiers & Qt::AltModifier) ? 1 : 0)
                      << std::endl;
            break;
        }
        case QEvent::InputMethod: {
            const auto *input = static_cast<QInputMethodEvent *>(event);
            if (!input->commitString().isEmpty())
                std::cout << "APP_TEXT text=" << input->commitString().toStdString() << std::endl;
            break;
        }
        default:
            return false;
        }
        return true;
    }

private:
    QWidget *m_root = nullptr;
};

int readPositiveInt(const QCommandLineParser &parser,
                    const QCommandLineOption &option,
                    int fallback)
{
    bool ok = false;
    const int value = parser.value(option).toInt(&ok);
    return ok && value > 0 ? value : fallback;
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HyRemote Widgets Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("HyRemote C++ API / Qt Widgets example"));
    parser.addHelpOption();
    QCommandLineOption portOption(QStringList{QStringLiteral("p"), QStringLiteral("port")},
                                  QStringLiteral("Loopback VNC port."),
                                  QStringLiteral("port"),
                                  QStringLiteral("5921"));
    QCommandLineOption inputOption(QStringLiteral("remote-input"),
                                   QStringLiteral("Explicitly enable remote input. Default is view-only."));
    QCommandLineOption secondsOption(QStringLiteral("test-seconds"),
                                     QStringLiteral("Exit after N seconds (CI/product-fit helper)."),
                                     QStringLiteral("seconds"),
                                     QStringLiteral("0"));
    QCommandLineOption policyTransitionOption(
        QStringLiteral("policy-transition-ms"),
        QStringLiteral("Acceptance helper: after the first viewer disconnect (or this watchdog), stop HyRemote, enable remote input while stopped, then restart without restarting the app."),
        QStringLiteral("milliseconds"),
        QStringLiteral("0"));
    parser.addOption(portOption);
    parser.addOption(inputOption);
    parser.addOption(secondsOption);
    parser.addOption(policyTransitionOption);
    parser.process(app);

    const int port = readPositiveInt(parser, portOption, 5921);
    if (port > 65535) {
        std::cerr << "invalid port" << std::endl;
        return 64;
    }
    const int testSeconds = readPositiveInt(parser, secondsOption, 0);
    const int policyTransitionMs = readPositiveInt(parser, policyTransitionOption, 0);
    const bool remoteInput = parser.isSet(inputOption);

    QWidget window;
    window.setWindowTitle(QStringLiteral("HyRemote Widgets Basic"));
    window.setFixedSize(360, 220);
    window.setStyleSheet(QStringLiteral("QWidget { background: #202733; color: #f2f4f8; }"
                                        "QPushButton { background: #3977d5; color: white; border: 0; }"
                                        "QLineEdit { background: #ffffff; color: #202733; padding: 4px; }"));

    auto *title = new QLabel(QStringLiteral("HyRemote · C++ API / Widgets"), &window);
    title->setGeometry(20, 18, 320, 32);

    auto *button = new QPushButton(QStringLiteral("Remote click target"), &window);
    button->setGeometry(20, 70, 140, 40);

    auto *status = new QLabel(&window);
    status->setGeometry(180, 66, 160, 48);
    status->setWordWrap(true);

    auto *editor = new QLineEdit(&window);
    editor->setGeometry(20, 130, 320, 36);
    editor->setPlaceholderText(QStringLiteral("Remote keyboard/text target"));

    auto *policy = new QLabel(remoteInput ? QStringLiteral("Remote control: enabled explicitly")
                                          : QStringLiteral("Remote control: view-only default"),
                              &window);
    policy->setGeometry(20, 178, 320, 24);

    QObject::connect(button, &QPushButton::clicked, &window, [editor] {
        editor->setFocus();
        std::cout << "APP_CLICKED" << std::endl;
    });

    InputProbe probe(&window);
    app.installEventFilter(&probe);

    window.show();
    editor->setFocus();
    QCoreApplication::processEvents();

    HyRemote::RemoteAccess remote(&window);
    remote.setPort(static_cast<quint16>(port));
    remote.setRemoteInputEnabled(remoteInput);
    if (!remote.start()) {
        const auto error = remote.lastError();
        std::cerr << "START_FAILED";
        if (error)
            std::cerr << " " << error->message.toStdString();
        std::cerr << std::endl;
        return 2;
    }

    std::size_t lastClientCount = remote.connectedClientCount();
    const auto updateStatus = [&] {
        status->setText(QStringLiteral("Listening 127.0.0.1:%1\nclients: %2")
                            .arg(port)
                            .arg(static_cast<qulonglong>(lastClientCount)));
    };
    updateStatus();
    std::cout << "CLIENT_COUNT " << lastClientCount << std::endl;

    bool acceptanceSawViewer = false;
    bool acceptancePolicyTransitionDone = false;
    QTimer policyTransitionTimer;
    policyTransitionTimer.setSingleShot(true);

    const auto applyAcceptancePolicyTransition = [&] {
        if (acceptancePolicyTransitionDone || policyTransitionMs <= 0 || remoteInput)
            return;

        acceptancePolicyTransitionDone = true;
        policyTransitionTimer.stop();
        remote.stop();
        lastClientCount = remote.connectedClientCount();
        updateStatus();
        std::cout << "CLIENT_COUNT " << lastClientCount << std::endl;
        std::cout << "POLICY_STOPPED" << std::endl;

        if (!remote.setRemoteInputEnabled(true)) {
            std::cerr << "POLICY_FAILED remoteInputEnabled" << std::endl;
            QCoreApplication::exit(3);
            return;
        }
        policy->setText(QStringLiteral("Remote control: enabled explicitly"));
        std::cout << "POLICY_INPUT true" << std::endl;
        std::cout << "POLICY_RESTART_REQUESTED" << std::endl;

        if (!remote.start()) {
            const auto error = remote.lastError();
            std::cerr << "POLICY_FAILED restart";
            if (error)
                std::cerr << " " << error->message.toStdString();
            std::cerr << std::endl;
            QCoreApplication::exit(4);
            return;
        }
        std::cout << "READY " << port << std::endl;
    };

    QTimer statusTimer;
    statusTimer.setInterval(100);
    statusTimer.setTimerType(Qt::CoarseTimer);
    QObject::connect(&statusTimer, &QTimer::timeout, &window, [&] {
        const std::size_t nextClientCount = remote.connectedClientCount();
        if (nextClientCount == lastClientCount)
            return;
        lastClientCount = nextClientCount;
        updateStatus();
        std::cout << "CLIENT_COUNT " << lastClientCount << std::endl;

        if (policyTransitionMs > 0 && !remoteInput && !acceptancePolicyTransitionDone) {
            if (lastClientCount > 0) {
                acceptanceSawViewer = true;
            } else if (acceptanceSawViewer) {
                applyAcceptancePolicyTransition();
            }
        }
    });
    statusTimer.start();

    if (policyTransitionMs > 0 && !remoteInput) {
        policyTransitionTimer.setInterval(policyTransitionMs);
        QObject::connect(&policyTransitionTimer,
                         &QTimer::timeout,
                         &window,
                         applyAcceptancePolicyTransition);
        policyTransitionTimer.start();
    }

    std::cout << "READY " << port << std::endl;

    if (testSeconds > 0)
        QTimer::singleShot(testSeconds * 1000, &app, &QCoreApplication::quit);

    const int result = app.exec();
    policyTransitionTimer.stop();
    statusTimer.stop();
    remote.stop();
    std::cout << "CLIENT_COUNT " << remote.connectedClientCount() << std::endl;
    std::cout << "STOPPED" << std::endl;
    return result;
}
