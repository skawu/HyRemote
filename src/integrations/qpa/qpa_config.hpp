#pragma once

#include <QHostAddress>
#include <QString>
#include <QStringList>

#include <optional>

namespace HyRemote::Qpa {

enum class SecurityProfile {
    Insecure,
    Authenticated,
    AuthenticatedEncrypted,
};

struct RemoteConfig
{
    // #174: an interface identity (QNetworkInterface::name()) selects interface mode and outranks the address
    // below; the two are mutually exclusive in the launch syntax.
    QString listenInterface;
    QHostAddress listenAddress = QHostAddress::AnyIPv4;
    quint16 port = static_cast<quint16>(HYREMOTE_DEFAULT_PORT);
    bool remoteInputEnabled = false;
    SecurityProfile securityProfile = SecurityProfile::Insecure;
    QString securityConfigFile;

    // #589: the same route-neutral description the Generic plugin uses; the shared Runtime policy owns the
    // defaults and the bounds, so the two zero-code routes cannot drift apart.
    QString watermarkText;
    QString watermarkPosition;
    std::optional<double> watermarkOpacity;
    std::optional<int> watermarkFontPointSize;
    std::optional<QString> watermarkColor;
};

// QPA owns only its launch/configuration vocabulary. Accepted HyRemote parameters are removed before
// the remainder is passed to the qualified native delegate; unknown HyRemote-prefixed values fail closed.
bool parseRemoteConfig(QStringList &parameters, RemoteConfig &config, QString &error);

// Runtime identity of the exact-Qt QPA qualification claim. An empty result means accepted.
QString runtimeIdentityError(const QString &runningVersion,
                             int expectedMajor,
                             int expectedMinor,
                             int expectedPatch);

}  // namespace HyRemote::Qpa
