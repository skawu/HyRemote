#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>

namespace {

void applyHyRemoteBranding()
{
    QGuiApplication::setWindowIcon(
        QIcon(QStringLiteral(":/hyremote/branding/huayan-logo-single.png")));
}

}  // namespace

Q_COREAPP_STARTUP_FUNCTION(applyHyRemoteBranding)
