#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HyRemote Installed QPA Consumer"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption secondsOption(QStringLiteral("test-seconds"),
                                     QStringLiteral("Exit after N seconds for package acceptance."),
                                     QStringLiteral("seconds"),
                                     QStringLiteral("0"));
    parser.addOption(secondsOption);
    parser.process(app);

    QWidget window;
    window.setWindowTitle(QStringLiteral("Ordinary Qt application"));
    auto *layout = new QVBoxLayout(&window);
    layout->addWidget(new QLabel(QStringLiteral("Qt-only application deployed with HyRemote QPA"), &window));
    auto *editor = new QLineEdit(&window);
    editor->setPlaceholderText(QStringLiteral("Local/native text input"));
    layout->addWidget(editor);
    window.resize(420, 180);
    window.show();
    editor->setFocus();

    bool ok = false;
    const int seconds = parser.value(secondsOption).toInt(&ok);
    if (ok && seconds > 0)
        QTimer::singleShot(seconds * 1000, &app, &QCoreApplication::quit);

    return app.exec();
}
