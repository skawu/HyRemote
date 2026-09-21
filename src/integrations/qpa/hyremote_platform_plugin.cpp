#include "qpa_config.hpp"

#include "automatic/automatic_access_config.hpp"
#include "automatic/automatic_access_controller.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QLibraryInfo>
#include <QtCore/QStringList>
#include <qpa/qplatformintegrationfactory_p.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformintegrationplugin.h>

#include <memory>
#include <utility>

QT_BEGIN_NAMESPACE
namespace {

static_assert(QT_VERSION == QT_VERSION_CHECK(HYREMOTE_QPA_QT_VERSION_MAJOR,
                                              HYREMOTE_QPA_QT_VERSION_MINOR,
                                              HYREMOTE_QPA_QT_VERSION_PATCH),
              "HyRemote QPA must be rebuilt for the exact supported Qt private ABI");

using AutomaticController = ::HyRemote::Runtime::Automatic::AccessController;
using AutomaticConfig = ::HyRemote::Runtime::Automatic::AccessConfig;

std::unique_ptr<AutomaticController> g_remoteController;
bool g_cleanupRegistered = false;

void cleanupRemoteController()
{
    if (g_remoteController)
        g_remoteController->stop();
    g_remoteController.reset();
}

QString referenceNativeDelegate()
{
#ifdef Q_OS_WIN
    return QStringLiteral("windows");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("xcb");
#else
    return {};
#endif
}

QString requestedDelegate(QStringList &parameters)
{
    const QString native = referenceNativeDelegate();
    QString selected = native;

    for (auto it = parameters.begin(); it != parameters.end();) {
        constexpr auto prefix = "delegate=";
        if (it->startsWith(QLatin1String(prefix), Qt::CaseInsensitive)) {
            selected = it->mid(static_cast<int>(sizeof(prefix) - 1)).trimmed().toLower();
            it = parameters.erase(it);
        } else {
            ++it;
        }
    }

    // This exact V1 qualification only decorates the reference native delegate. Later qualified
    // packages may select wayland/eglfs/etc. explicitly; this payload never guesses or falls back.
    if (native.isEmpty() || selected != native)
        return {};
    return selected;
}

::HyRemote::Runtime::SecurityProfile runtimeSecurityProfile(::HyRemote::Qpa::SecurityProfile profile)
{
    switch (profile) {
    case ::HyRemote::Qpa::SecurityProfile::Insecure:
        return ::HyRemote::Runtime::SecurityProfile::Insecure;
    case ::HyRemote::Qpa::SecurityProfile::Authenticated:
        return ::HyRemote::Runtime::SecurityProfile::Authenticated;
    case ::HyRemote::Qpa::SecurityProfile::AuthenticatedEncrypted:
        return ::HyRemote::Runtime::SecurityProfile::AuthenticatedEncrypted;
    }
    return ::HyRemote::Runtime::SecurityProfile::Insecure;
}

AutomaticConfig automaticConfig(::HyRemote::Qpa::RemoteConfig config)
{
    AutomaticConfig result;
    result.listenAddress = std::move(config.listenAddress);
    result.port = config.port;
    result.remoteInputEnabled = config.remoteInputEnabled;
    result.securityProfile = runtimeSecurityProfile(config.securityProfile);
    result.securityConfigFile = std::move(config.securityConfigFile);
    return result;
}

bool armRemoteRuntime(::HyRemote::Qpa::RemoteConfig config)
{
    // QPA integration creation occurs before Qt allows a platform plugin to create the event
    // dispatcher. The shared controller therefore installs only passive application observation here;
    // it does not schedule its initial zero-timer. Real Show/geometry/focus events occur after the
    // native integration is established and drive the normal refresh path.
    auto controller = std::make_unique<AutomaticController>(automaticConfig(std::move(config)));
    if (!controller->start(false))
        return false;

    g_remoteController = std::move(controller);
    if (!g_cleanupRegistered) {
        qAddPostRoutine(&cleanupRemoteController);
        g_cleanupRegistered = true;
    }
    return true;
}

class HyRemotePlatformIntegrationPlugin final : public QPlatformIntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformIntegrationFactoryInterface_iid FILE "hyremote.json")

public:
    QPlatformIntegration *create(const QString &key,
                                 const QStringList &paramList,
                                 int &argc,
                                 char **argv) override
    {
        if (key.compare(QStringLiteral("hyremote"), Qt::CaseInsensitive) != 0)
            return nullptr;

        const QString identityError =
            ::HyRemote::Qpa::runtimeIdentityError(QString::fromLatin1(qVersion()),
                                                 HYREMOTE_QPA_QT_VERSION_MAJOR,
                                                 HYREMOTE_QPA_QT_VERSION_MINOR,
                                                 HYREMOTE_QPA_QT_VERSION_PATCH);
        if (!identityError.isEmpty()) {
            qCritical().noquote() << identityError;
            return nullptr;
        }

        QStringList delegateParameters = paramList;
        ::HyRemote::Qpa::RemoteConfig remoteConfig;
        QString remoteConfigError;
        if (!::HyRemote::Qpa::parseRemoteConfig(delegateParameters, remoteConfig, remoteConfigError)) {
            qWarning() << "HyRemote QPA rejected remote configuration:" << remoteConfigError;
            return nullptr;
        }

        const QString delegateName = requestedDelegate(delegateParameters);
        if (delegateName.isEmpty()) {
            qCritical().noquote()
                << QStringLiteral("HyRemote QPA rejected the requested platform delegate; this payload only "
                                  "decorates '%1' for the exact Qt %2.%3.%4 private ABI, and never falls back to "
                                  "another delegate.")
                       .arg(referenceNativeDelegate())
                       .arg(HYREMOTE_QPA_QT_VERSION_MAJOR)
                       .arg(HYREMOTE_QPA_QT_VERSION_MINOR)
                       .arg(HYREMOTE_QPA_QT_VERSION_PATCH);
            return nullptr;
        }

        // Factory Trampoline: create the real native Qt platform integration and return that exact
        // object to QGuiApplication. HyRemote does not wrap/reimplement QPlatformIntegration and
        // therefore does not have to mirror protected virtuals or backend-specific native interfaces.
        QPlatformIntegration *delegate =
            QPlatformIntegrationFactory::create(delegateName,
                                                delegateParameters,
                                                argc,
                                                argv);
        if (!delegate) {
            qCritical().noquote()
                << QStringLiteral("HyRemote QPA could not create the native delegate '%1' required for the "
                                  "exact Qt %2.%3.%4 private ABI; platform plugin path '%5'; library paths: %6.")
                       .arg(delegateName)
                       .arg(HYREMOTE_QPA_QT_VERSION_MAJOR)
                       .arg(HYREMOTE_QPA_QT_VERSION_MINOR)
                       .arg(HYREMOTE_QPA_QT_VERSION_PATCH)
                       .arg(QDir::toNativeSeparators(QLibraryInfo::path(QLibraryInfo::PluginsPath)),
                            QCoreApplication::libraryPaths().join(QStringLiteral(", ")));
            return nullptr;
        }

        // Remote capability is sidecar behavior. A failure to arm HyRemote must never invalidate the
        // native QPA that Qt successfully created; the local application remains authoritative/live.
        cleanupRemoteController();
        if (!armRemoteRuntime(std::move(remoteConfig))) {
            qWarning() << "HyRemote QPA could not arm automatic remote access; native delegate remains active:"
                       << delegateName;
        }

        qInfo() << "HyRemote QPA Factory Trampoline active; native delegate:" << delegateName;
        return delegate;
    }
};

}  // namespace
QT_END_NAMESPACE

#include "hyremote_platform_plugin.moc"
