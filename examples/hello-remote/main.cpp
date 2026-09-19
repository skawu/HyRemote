// The smallest possible HyRemote integration: show a window and make it viewable remotely.
//
// This is an onboarding example, not release evidence - the E1-E6 matrix in ../README.md is what the release
// process accepts. It exists because those examples are acceptance instruments: each carries a READY /
// CLIENT_COUNT / POLICY_* protocol and an input probe, so someone learning the product would have to read that
// scaffolding to find the few lines that actually matter. Everything below is the normal consumer path:
// construct with the window as target, start, stop.

#include <HyRemote/RemoteAccess.h>

#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <iostream>
#include <string>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle(QStringLiteral("HyRemote hello"));
    window.resize(360, 160);
    auto *layout = new QVBoxLayout(&window);
    layout->addWidget(new QLabel(QStringLiteral("This window is shared by HyRemote.")));
    window.show();

    // Construction is inert. start() opens the service, and the safe defaults apply: loopback only, port 5921,
    // and remote input disabled, so a viewer can look but not touch.
    HyRemote::RemoteAccess remote(&window);
    if (!remote.start()) {
        const auto error = remote.lastError();
        std::cerr << "HyRemote did not start: "
                  << (error ? error->message.toStdString() : std::string("no diagnostic reported")) << std::endl;
        return 2;
    }

    // std::endl rather than '\n': when this output is redirected to a file, an unflushed line is lost if the
    // process is stopped, which is exactly what a scripted smoke check of this example does.
    std::cout << "HyRemote is listening on 127.0.0.1:" << remote.port()
              << " - connect a VNC viewer, view-only." << std::endl;

    const int result = app.exec();
    remote.stop();
    return result;
}
