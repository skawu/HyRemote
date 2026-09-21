#include "../qpa_config.hpp"

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
    using HyRemote::Qpa::runtimeIdentityError;

    {
        QStringList parameters{QStringLiteral("delegate=xcb"), QStringLiteral("display=:99")};
        RemoteConfig config;
        QString error;
        if (!check(parseRemoteConfig(parameters, config, error), "default config parses")
            || !check(error.isEmpty(), "default config has no error")
            || !check(config.listenAddress == QHostAddress::LocalHost, "default address is loopback")
            || !check(config.port == HYREMOTE_DEFAULT_PORT, "default port is the configured HYREMOTE_DEFAULT_PORT")
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
        QStringLiteral("hyremote-typo=1"),
        QStringLiteral("hyremote-portt=5921"),
        QStringLiteral("hyremote-"),
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

    if (!check(runtimeIdentityError(QStringLiteral("6.8.3"), 6, 8, 3).isEmpty(),
               "exact runtime identity is accepted")) {
        return 4;
    }

    struct RejectedRuntime
    {
        const char *version;
        const char *label;
    };
    const RejectedRuntime rejectedRuntimes[] = {
        {"6.8.2", "older patch"},
        {"6.8.4", "newer patch"},
        {"6.7.3", "older minor"},
        {"6.9.3", "newer minor"},
        {"5.15.2", "older major"},
        {"6.8", "incomplete"},
        {"not-a-version", "unreadable"},
        {"", "empty"},
    };
    for (const RejectedRuntime &runtime : rejectedRuntimes) {
        const QString diagnostic = runtimeIdentityError(QString::fromLatin1(runtime.version), 6, 8, 3);
        if (!check(!diagnostic.isEmpty(), "mismatched runtime is rejected")
            || !check(diagnostic.contains(QStringLiteral("6.8.3")),
                      "diagnostic names the required version")) {
            return 5;
        }
    }

    if (!check(runtimeIdentityError(QStringLiteral("6.8.4"), 6, 8, 3).contains(QStringLiteral("6.8.4")),
               "diagnostic names the running version")) {
        return 6;
    }

    if (!check(runtimeIdentityError(QStringLiteral("6.8.4"), 6, 8, 4).isEmpty(),
               "requirement follows the built expectation")
        || !check(!runtimeIdentityError(QStringLiteral("6.8.3"), 6, 8, 4).isEmpty(),
                  "requirement follows the built expectation for mismatch")) {
        return 7;
    }

    if (!check(runtimeIdentityError(QStringLiteral("6.8.3-debian"), 6, 8, 3).isEmpty(),
               "decorated exact runtime is accepted")
        || !check(!runtimeIdentityError(QStringLiteral("6.8.4-debian"), 6, 8, 3).isEmpty(),
                  "decorated mismatched runtime is rejected")) {
        return 8;
    }

    std::cout << "PASS: QPA config safe defaults, explicit security overrides and exact runtime identity\n";
    return 0;
}
