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
    // #761: the explicit off switch, so clearing a stored watermark is a stated intent instead of a side effect of
    // leaving the text out.
    bool watermarkOff = false;
};

// QPA owns only its launch/configuration vocabulary. Accepted HyRemote parameters are removed before
// the remainder is passed to the qualified native delegate; unknown HyRemote-prefixed values fail closed.
bool parseRemoteConfig(QStringList &parameters, RemoteConfig &config, QString &error);

// #774: delegate selection is part of the QPA launch grammar. Keep one
// parser used by the actual private-ABI plugin and its focused unit test.
// "delegate=windows" on Windows (or "delegate=xcb" on Linux) must not
// silently become a different token after prefix removal.
QString requestedNativeDelegate(QStringList &parameters, const QString &native);

// Runtime identity of the exact-Qt QPA qualification claim. An empty result means accepted.
QString runtimeIdentityError(const QString &runningVersion,
                             int expectedMajor,
                             int expectedMinor,
                             int expectedPatch);

}  // namespace HyRemote::Qpa
