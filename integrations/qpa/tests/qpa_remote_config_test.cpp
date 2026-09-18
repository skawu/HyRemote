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
    using HyRemote::Qpa::parseRemoteConfig;

    {
        QStringList parameters{QStringLiteral("delegate=xcb"), QStringLiteral("display=:99")};
        RemoteConfig config;
        QString error;
        if (!check(parseRemoteConfig(parameters, config, error), "default config parses")
            || !check(error.isEmpty(), "default config has no error")
            || !check(config.listenAddress == QHostAddress::LocalHost, "default address is loopback")
            || !check(config.port == HYREMOTE_DEFAULT_PORT, "default port is the configured HYREMOTE_DEFAULT_PORT")
            || !check(!config.remoteInputEnabled, "remote input defaults off")
            || !check(parameters == QStringList{QStringLiteral("delegate=xcb"), QStringLiteral("display=:99")},
                      "native delegate parameters are preserved")) {
            return 1;
        }
    }

    {
        QStringList parameters{QStringLiteral("hyremote-address=0.0.0.0"),
                               QStringLiteral("hyremote-port=5999"),
                               QStringLiteral("hyremote-input=on"),
                               QStringLiteral("darkmode=2")};
        RemoteConfig config;
        QString error;
        if (!check(parseRemoteConfig(parameters, config, error), "explicit config parses")
            || !check(config.listenAddress == QHostAddress::AnyIPv4, "explicit wildcard remains explicit")
            || !check(config.port == 5999, "explicit port is applied")
            || !check(config.remoteInputEnabled, "remote input can be explicitly enabled")
            || !check(parameters == QStringList{QStringLiteral("darkmode=2")},
                      "HyRemote parameters are removed before native delegate creation")) {
            return 2;
        }
    }

    const QStringList invalidParameters{
        QStringLiteral("hyremote-address=not-an-address"),
        QStringLiteral("hyremote-port=0"),
        QStringLiteral("hyremote-port=65536"),
        QStringLiteral("hyremote-input=maybe"),
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

    std::cout << "PASS: QPA remote config safe defaults and explicit overrides\n";
    return 0;
}
