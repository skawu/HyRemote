#include <HyRemote/RemoteAccess.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QEvent>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQmlError>
#include <QQuickView>
#include <QTimer>
#include <QWheelEvent>

#include <iostream>

namespace {

class InputProbe final : public QObject
{
public:
    explicit InputProbe(QQuickWindow *window)
        : m_window(window)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (!m_window || watched != m_window)
            return false;

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
            break;
        }
        return false;
    }

private:
    QQuickWindow *m_window = nullptr;
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
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HyRemote Quick Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("HyRemote Embedded C++ API / Qt Quick example"));
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

    QQuickView view;
    view.setTitle(QStringLiteral("HyRemote Quick Basic"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(360, 220);
    view.setInitialProperties({{QStringLiteral("remoteControlEnabled"), remoteInput}});
    view.setSource(QUrl(QStringLiteral("qrc:/hyremote/quick-basic/Main.qml")));
    if (view.status() == QQuickView::Error) {
        for (const QQmlError &error : view.errors())
            std::cerr << error.toString().toStdString() << std::endl;
        return 65;
    }

    InputProbe probe(&view);
    view.installEventFilter(&probe);
    view.show();
    QCoreApplication::processEvents();

    HyRemote::RemoteAccess remote(&view);
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

    std::cout << "READY " << port << std::endl;
    if (testSeconds > 0)
        QTimer::singleShot(testSeconds * 1000, &app, &QCoreApplication::quit);

    const int result = app.exec();
    remote.stop();
    std::cout << "STOPPED" << std::endl;
    return result;
}
