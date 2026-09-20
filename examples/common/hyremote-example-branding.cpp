#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>

namespace {

void applyHyRemoteExampleBranding()
{
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/hyremote/branding/logo.png")));
}

}  // namespace

Q_COREAPP_STARTUP_FUNCTION(applyHyRemoteExampleBranding)
