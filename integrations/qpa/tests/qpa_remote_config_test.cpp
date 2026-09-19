#include "../hyremote_qpa_remote_controller.hpp"

#include <QHostAddress>
#include <QStringList>

#include <iostream>

namespace {

bool check(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

}  // namespace

int main()
{
    using HyRemote::Qpa::RemoteConfig;
    using HyRemote::Qpa::SecurityProfile;
    using HyRemote::Qpa::parseRemoteConfig;

    {
        QStringList parameters{QStringLiteral("delegate=xcb"), QStringLiteral("display=:99")};
        RemoteConfig config;
        QString error;
        if (!check(parseRemoteConfig(parameters, config, error), "default config parses")
            || !check(error.isEmpty(), "default config has no error")
            || !check(config.listenAddress == QHostAddress::LocalHost, "default address is loopback")
            || !check(config.port == 5900, "default port is 5900")
            || !check(!config.remoteInputEnabled, "remote input defaults off")
            || !check(config.securityProfile == SecurityProfile::Insecure,
                      "security defaults to explicit insecure compatibility profile")
            || !check(config.securityConfigFile.isEmpty(), "security descriptor defaults empty")
            || !check(parameters == QStringList{QStringLiteral("delegate=xcb"), QStringLiteral("display=:99")},
                      "native delegate parameters are preserved")) {
            return 1;
        }
    }

    {
        QStringList parameters{QStringLiteral("hyremote-address=0.0.0.0"),
                               QStringLiteral("hyremote-port=5999"),
                               QStringLiteral("hyremote-input=on"),
                               QStringLiteral("hyremote-security=authenticated-encrypted"),
                               QStringLiteral("hyremote-security-config=C:/ProgramData/HyRemote/security.conf"),
                               QStringLiteral("darkmode=2")};
        RemoteConfig config;
        QString error;
        if (!check(parseRemoteConfig(parameters, config, error), "explicit config parses")
            || !check(config.listenAddress == QHostAddress::AnyIPv4, "explicit wildcard remains explicit")
            || !check(config.port == 5999, "explicit port is applied")
            || !check(config.remoteInputEnabled, "remote input can be explicitly enabled")
            || !check(config.securityProfile == SecurityProfile::AuthenticatedEncrypted,
                      "encrypted authenticated profile is explicit")
            || !check(config.securityConfigFile
                          == QStringLiteral("C:/ProgramData/HyRemote/security.conf"),
                      "only a non-secret descriptor path is carried in the platform config")
            || !check(parameters == QStringList{QStringLiteral("darkmode=2")},
                      "HyRemote parameters are removed before native delegate creation")) {
            return 2;
        }
    }

    {
        QStringList parameters{QStringLiteral("hyremote-security=authenticated"),
                               QStringLiteral("hyremote-security-config=/etc/hyremote/security.conf")};
        RemoteConfig config;
        QString error;
        if (!check(parseRemoteConfig(parameters, config, error), "authenticated profile parses")
            || !check(config.securityProfile == SecurityProfile::Authenticated,
                      "authenticated profile is distinct from encrypted")
            || !check(parameters.isEmpty(), "security parameters are consumed")) {
            return 4;
        }
    }

    const QStringList invalidParameters{
        QStringLiteral("hyremote-address=not-an-address"),
        QStringLiteral("hyremote-port=0"),
        QStringLiteral("hyremote-port=65536"),
        QStringLiteral("hyremote-input=maybe"),
        QStringLiteral("hyremote-security=maybe"),
        QStringLiteral("hyremote-security"),
        QStringLiteral("hyremote-security-config"),
        QStringLiteral("hyremote-security-config="),
    };
    for (const QString &parameter : invalidParameters) {
        QStringList parameters{parameter};
        RemoteConfig config;
        QString error;
        if (!check(!parseRemoteConfig(parameters, config, error), "invalid config fails closed")
            || !check(!error.isEmpty(), "invalid config reports a diagnostic")) {
            return 3;
        }
    }

    std::cout << "PASS: QPA remote config safe defaults and explicit security overrides\n";
    return 0;
}
