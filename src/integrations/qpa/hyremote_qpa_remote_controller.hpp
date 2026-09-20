#pragma once

#include <QHostAddress>
#include <QString>
#include <QStringList>

#include <memory>

namespace HyRemote::Runtime::Automatic {
class AccessController;
}

namespace HyRemote::Qpa {

enum class SecurityProfile {
    Insecure,
    Authenticated,
    AuthenticatedEncrypted,
};

struct RemoteConfig
{
    QHostAddress listenAddress = QHostAddress::LocalHost;
    quint16 port = static_cast<quint16>(HYREMOTE_DEFAULT_PORT);
    bool remoteInputEnabled = false;
    SecurityProfile securityProfile = SecurityProfile::Insecure;
    QString securityConfigFile;
};

// QPA owns only its launch/configuration vocabulary. Accepted HyRemote parameters are removed before
// the remainder is passed to the qualified native delegate; unknown HyRemote-prefixed values fail closed.
bool parseRemoteConfig(QStringList &parameters, RemoteConfig &config, QString &error);

// Runtime identity of the exact-Qt QPA qualification claim. An empty result means accepted.
QString runtimeIdentityError(const QString &runningVersion,
                             int expectedMajor,
                             int expectedMinor,
                             int expectedPatch);

// Thin QPA bootstrap adapter over the common application-level automatic access controller. QPA no
// longer owns surface discovery, composition, input routing or RemoteAccess lifecycle semantics.
class RemoteController final
{
public:
    explicit RemoteController(RemoteConfig config);
    ~RemoteController();

    RemoteController(const RemoteController &) = delete;
    RemoteController &operator=(const RemoteController &) = delete;

    bool start();
    void stop() noexcept;

private:
    std::unique_ptr<::HyRemote::Runtime::Automatic::AccessController> m_controller;
};

}  // namespace HyRemote::Qpa
