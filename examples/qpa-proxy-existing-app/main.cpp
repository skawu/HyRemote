#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

class ExistingOperationsWindow final : public QMainWindow
{
public:
    ExistingOperationsWindow()
    {
        setWindowTitle(QStringLiteral("Existing Qt Operations Console"));
        resize(860, 560);

        auto *central = new QWidget(this);
        auto *layout = new QVBoxLayout(central);

        auto *intro = new QLabel(
            QStringLiteral("Operations console for local equipment monitoring and command entry."),
            central);
        intro->setWordWrap(true);
        layout->addWidget(intro);

        auto *form = new QFormLayout;
        auto *asset = new QLineEdit(QStringLiteral("Conveyor-07"), central);
        auto *setpoint = new QSpinBox(central);
        setpoint->setRange(0, 1000);
        setpoint->setValue(420);
        auto *load = new QSlider(Qt::Horizontal, central);
        load->setRange(0, 100);
        load->setValue(58);
        auto *maintenance = new QCheckBox(QStringLiteral("Maintenance requested"), central);
        form->addRow(QStringLiteral("Asset"), asset);
        form->addRow(QStringLiteral("Command setpoint"), setpoint);
        form->addRow(QStringLiteral("Process load"), load);
        form->addRow(QString{}, maintenance);
        layout->addLayout(form);

        auto *progress = new QProgressBar(central);
        progress->setRange(0, 100);
        progress->setValue(load->value());
        QObject::connect(load, &QSlider::valueChanged, progress, &QProgressBar::setValue);
        layout->addWidget(progress);

        auto *notes = new QPlainTextEdit(central);
        notes->setPlaceholderText(QStringLiteral("Enter shift handover notes"));
        notes->setPlainText(QStringLiteral("Shift handover: inspect gearbox temperature trend."));
        layout->addWidget(notes, 1);

        auto *buttons = new QHBoxLayout;
        auto *openDialog = new QPushButton(QStringLiteral("Open diagnostics window"), central);
        auto *apply = new QPushButton(QStringLiteral("Apply local command"), central);
        buttons->addWidget(openDialog);
        buttons->addWidget(apply);
        buttons->addStretch(1);
        layout->addLayout(buttons);

        setCentralWidget(central);

        auto *fileMenu = menuBar()->addMenu(QStringLiteral("&Application"));
        auto *diagnosticsAction = fileMenu->addAction(QStringLiteral("Open diagnostics"));
        fileMenu->addSeparator();
        fileMenu->addAction(QStringLiteral("Quit"), qApp, &QApplication::quit);

        statusBar()->showMessage(QStringLiteral("Local application running"));
        QObject::connect(apply, &QPushButton::clicked, this, [this, asset, setpoint] {
            statusBar()->showMessage(
                QStringLiteral("Applied %1 to %2 locally")
                    .arg(setpoint->value())
                    .arg(asset->text()),
                3000);
        });

        const auto showDiagnostics = [this] {
            auto *dialog = new QDialog(this, Qt::Window);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setWindowTitle(QStringLiteral("Independent diagnostics window"));
            dialog->resize(420, 240);

            auto *dialogLayout = new QVBoxLayout(dialog);
            auto *headline = new QLabel(QStringLiteral("Live diagnostics"), dialog);
            auto *detail = new QLabel(
                QStringLiteral("Secondary diagnostics view for device state and operator acknowledgement."),
                dialog);
            detail->setWordWrap(true);
            auto *field = new QLineEdit(QStringLiteral("Acknowledgement text"), dialog);
            auto *close = new QPushButton(QStringLiteral("Close"), dialog);
            dialogLayout->addWidget(headline);
            dialogLayout->addWidget(detail);
            dialogLayout->addWidget(field);
            dialogLayout->addStretch(1);
            dialogLayout->addWidget(close);
            QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::close);
            dialog->show();
            dialog->raise();
            dialog->activateWindow();
        };

        QObject::connect(openDialog, &QPushButton::clicked, this, showDiagnostics);
        QObject::connect(diagnosticsAction, &QAction::triggered, this, showDiagnostics);
    }
};

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Existing Qt Operations Console"));

    // This is an ordinary application-owned command-line option, not a HyRemote integration API.
    // It gives automated package acceptance a bounded lifetime while preserving the defining E4
    // contract: this source and executable target remain Qt-only and know nothing about HyRemote.
    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption secondsOption(QStringLiteral("test-seconds"),
                                     QStringLiteral("Exit after N seconds for automated application acceptance."),
                                     QStringLiteral("seconds"),
                                     QStringLiteral("0"));
    parser.addOption(secondsOption);
    parser.process(app);

    ExistingOperationsWindow window;
    window.show();

    bool ok = false;
    const int seconds = parser.value(secondsOption).toInt(&ok);
    if (ok && seconds > 0)
        QTimer::singleShot(seconds * 1000, &app, &QCoreApplication::quit);

    return app.exec();
}
