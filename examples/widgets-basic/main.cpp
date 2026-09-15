#include <HyRemote/RemoteAccess.h>

#include <QApplication>
#include <QCheckBox>
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

        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            std::cout << "APP_POINTER x=" << mouse->position().x()
                      << " y=" << mouse->position().y() << std::endl;
            break;
        }
        case QEvent::KeyPress: {
            const auto *key = static_cast<QKeyEvent *>(event);
            std::cout << "APP_KEY key=" << key->key() << std::endl;
            break;
        }
        case QEvent::InputMethod: {
            const auto *input = static_cast<QInputMethodEvent *>(event);
            if (!input->commitString().isEmpty())
                std::cout << "APP_TEXT text=" << input->commitString().toStdString() << std::endl;
            break;
        }
        default:
            break;
        }
        return false;
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
    parser.setApplicationDescription(QStringLiteral("HyRemote Embedded C++ API / Qt Widgets example"));
    parser.addHelpOption();
    QCommandLineOption portOption(QStringList{QStringLiteral("p"), QStringLiteral("port")},
                                  QStringLiteral("Loopback VNC port."),
                                  QStringLiteral("port"),
                                  QStringLiteral("5900"));
    QCommandLineOption inputOption(QStringLiteral("remote-input"),
                                   QStringLiteral("Explicitly enable remote input. Default is view-only."));
    QCommandLineOption secondsOption(QStringLiteral("test-seconds"),
                                     QStringLiteral("Exit after N seconds (CI/product-fit helper)."),
                                     QStringLiteral("seconds"),
                                     QStringLiteral("0"));
    parser.addOption(portOption);
    parser.addOption(inputOption);
    parser.addOption(secondsOption);
    parser.process(app);

    const int port = readPositiveInt(parser, portOption, 5900);
    if (port > 65535) {
        std::cerr << "invalid port" << std::endl;
        return 64;
    }
    const int testSeconds = readPositiveInt(parser, secondsOption, 0);
    const bool remoteInput = parser.isSet(inputOption);

    QWidget window;
    window.setWindowTitle(QStringLiteral("HyRemote Widgets Basic"));
    window.setFixedSize(360, 220);
    window.setStyleSheet(QStringLiteral("QWidget { background: #202733; color: #f2f4f8; }"
                                        "QPushButton { background: #3977d5; color: white; border: 0; }"
                                        "QLineEdit { background: #ffffff; color: #202733; padding: 4px; }"));

    auto *title = new QLabel(QStringLiteral("HyRemote · Embedded C++ / Widgets"), &window);
    title->setGeometry(20, 18, 320, 32);

    auto *button = new QPushButton(QStringLiteral("Remote click target"), &window);
    button->setGeometry(20, 70, 140, 40);

    auto *status = new QLabel(&window);
    status->setGeometry(180, 70, 160, 40);
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

    status->setText(QStringLiteral("Running on 127.0.0.1:%1").arg(port));
    std::cout << "READY " << port << std::endl;

    if (testSeconds > 0)
        QTimer::singleShot(testSeconds * 1000, &app, &QCoreApplication::quit);

    const int result = app.exec();
    remote.stop();
    std::cout << "STOPPED" << std::endl;
    return result;
}
