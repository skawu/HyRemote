#include "hyremote_qpa_remote_controller.hpp"

#include "automatic/automatic_access_config.hpp"
#include "automatic/automatic_access_controller.hpp"

#include <utility>

namespace HyRemote::Qpa {
namespace {

bool parseBoolean(const QString &text, bool &value)
{
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("1") || normalized == QStringLiteral("true")
        || normalized == QStringLiteral("on") || normalized == QStringLiteral("yes")) {
        value = true;
        return true;
    }
    if (normalized == QStringLiteral("0") || normalized == QStringLiteral("false")
        || normalized == QStringLiteral("off") || normalized == QStringLiteral("no")) {
        value = false;
        return true;
    }
    return false;
}

bool parseSecurityProfile(const QString &text, SecurityProfile &profile)
{
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("insecure")) {
        profile = SecurityProfile::Insecure;
        return true;
    }
    if (normalized == QStringLiteral("authenticated")) {
        profile = SecurityProfile::Authenticated;
        return true;
    }
    if (normalized == QStringLiteral("authenticated-encrypted")) {
        profile = SecurityProfile::AuthenticatedEncrypted;
        return true;
    }
    return false;
}

Runtime::SecurityProfile runtimeSecurityProfile(SecurityProfile profile)
{
    switch (profile) {
    case SecurityProfile::Insecure:
        return Runtime::SecurityProfile::Insecure;
    case SecurityProfile::Authenticated:
        return Runtime::SecurityProfile::Authenticated;
    case SecurityProfile::AuthenticatedEncrypted:
        return Runtime::SecurityProfile::AuthenticatedEncrypted;
    }
    return Runtime::SecurityProfile::Insecure;
}

QString leadingDigits(const QString &text)
{
    int length = 0;
    while (length < text.size() && text.at(length).isDigit())
        ++length;
    return text.left(length);
}

Runtime::Automatic::AccessConfig automaticConfig(RemoteConfig config)
{
    Runtime::Automatic::AccessConfig result;
    result.listenAddress = std::move(config.listenAddress);
    result.port = config.port;
    result.remoteInputEnabled = config.remoteInputEnabled;
    result.securityProfile = runtimeSecurityProfile(config.securityProfile);
    result.securityConfigFile = std::move(config.securityConfigFile);
    return result;
}

}  // namespace

QString runtimeIdentityError(const QString &runningVersion,
                             int expectedMajor,
                             int expectedMinor,
                             int expectedPatch)
{
    const QString expected = QStringLiteral("%1.%2.%3")
                                 .arg(expectedMajor)
                                 .arg(expectedMinor)
                                 .arg(expectedPatch);
    const QString requirement = QStringLiteral("HyRemote Transparent QPA Proxy requires the exact Qt %1 "
                                               "private ABI at run time").arg(expected);
    const QString remedy = QStringLiteral("Install a payload qualified for the running Qt, or run the Qt "
                                          "this payload was built for.");

    const QStringList components = runningVersion.trimmed().split(QLatin1Char('.'));
    if (components.size() >= 3) {
        int actual[3] = {0, 0, 0};
        bool parsed = true;
        for (int index = 0; index < 3; ++index) {
            bool ok = false;
            actual[index] = leadingDigits(components.at(index)).toInt(&ok);
            if (!ok) {
                parsed = false;
                break;
            }
        }

        if (parsed && actual[0] == expectedMajor && actual[1] == expectedMinor
            && actual[2] == expectedPatch) {
            return {};
        }

        if (parsed) {
            return requirement
                   + QStringLiteral(", but this process is running Qt %1 (%2). %3")
                         .arg(QStringLiteral("%1.%2.%3").arg(actual[0]).arg(actual[1]).arg(actual[2]),
                              runningVersion.trimmed(),
                              remedy);
        }
    }

    return requirement
           + QStringLiteral(", but the running Qt reported the unusable version '%1'. %2")
                 .arg(runningVersion.trimmed(), remedy);
}

bool parseRemoteConfig(QStringList &parameters, RemoteConfig &config, QString &error)
{
    RemoteConfig parsed;

    for (auto it = parameters.begin(); it != parameters.end();) {
        const QString parameter = *it;
        const int separator = parameter.indexOf(QLatin1Char('='));
        const QString key = (separator >= 0 ? parameter.left(separator) : parameter).trimmed().toLower();
        const QString value = separator >= 0 ? parameter.mid(separator + 1).trimmed() : QString{};

        if (key == QStringLiteral("hyremote-address")) {
            if (separator < 0 || value.isEmpty()) {
                error = QStringLiteral("hyremote-address requires an explicit IP address");
                return false;
            }
            const QHostAddress address(value);
            if (address.isNull()) {
                error = QStringLiteral("invalid hyremote-address: %1").arg(value);
                return false;
            }
            parsed.listenAddress = address;
            it = parameters.erase(it);
            continue;
        }

        if (key == QStringLiteral("hyremote-port")) {
            bool ok = false;
            const int port = value.toInt(&ok);
            if (separator < 0 || !ok || port < 1 || port > 65535) {
                error = QStringLiteral("hyremote-port must be in the range 1..65535");
                return false;
            }
            parsed.port = static_cast<quint16>(port);
            it = parameters.erase(it);
            continue;
        }

        if (key == QStringLiteral("hyremote-input")) {
            bool enabled = false;
            if (separator < 0 || !parseBoolean(value, enabled)) {
                error = QStringLiteral("hyremote-input must be one of 0/1, false/true, off/on or no/yes");
                return false;
            }
            parsed.remoteInputEnabled = enabled;
            it = parameters.erase(it);
            continue;
        }

        if (key == QStringLiteral("hyremote-security")) {
            if (separator < 0 || !parseSecurityProfile(value, parsed.securityProfile)) {
                error = QStringLiteral(
                    "hyremote-security must be insecure, authenticated or authenticated-encrypted");
                return false;
            }
            it = parameters.erase(it);
            continue;
        }

        if (key == QStringLiteral("hyremote-security-config")) {
            if (separator < 0 || value.isEmpty()) {
                error = QStringLiteral("hyremote-security-config requires a non-empty descriptor path");
                return false;
            }
            parsed.securityConfigFile = value;
            it = parameters.erase(it);
            continue;
        }

        if (key.startsWith(QStringLiteral("hyremote-"))) {
            error = QStringLiteral("unknown HyRemote platform parameter: %1").arg(key);
            return false;
        }

        ++it;
    }

    config = std::move(parsed);
    error.clear();
    return true;
}

RemoteController::RemoteController(RemoteConfig config)
    : m_controller(std::make_unique<Runtime::Automatic::AccessController>(
          automaticConfig(std::move(config))))
{
}

RemoteController::~RemoteController()
{
    stop();
}

bool RemoteController::start()
{
    return m_controller && m_controller->start();
}

void RemoteController::stop() noexcept
{
    if (m_controller)
        m_controller->stop();
}

}  // namespace HyRemote::Qpa
