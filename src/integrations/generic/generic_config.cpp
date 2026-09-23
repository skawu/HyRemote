#include "generic_config.hpp"

#include <QHostAddress>
#include <QStringList>

namespace HyRemote::Generic {
namespace {

using Runtime::Automatic::AccessConfig;
using Runtime::SecurityProfile;

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

}  // namespace

bool parseSpecification(const QString &specification, AccessConfig &config, QString &error)
{
    AccessConfig parsed;
    // The two binding keys describe the same decision, so accepting both would leave the mode ambiguous. They are
    // tracked separately from the parsed values because an explicit address is only "seen" when the user wrote one.
    bool addressSeen = false;
    bool interfaceSeen = false;
    if (specification.trimmed().isEmpty()) {
        config = parsed;
        error.clear();
        return true;
    }

    const QStringList fields = specification.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &field : fields) {
        const int separator = field.indexOf(QLatin1Char('='));
        if (separator <= 0) {
            error = QStringLiteral("Generic Plugin specification fields must use key=value syntax: %1")
                        .arg(field.trimmed());
            return false;
        }

        const QString key = field.left(separator).trimmed().toLower();
        const QString value = field.mid(separator + 1).trimmed();
        if (value.isEmpty()) {
            error = QStringLiteral("Generic Plugin option '%1' requires a value").arg(key);
            return false;
        }

        if (key == QStringLiteral("address")) {
            if (interfaceSeen) {
                error = QStringLiteral("options 'address' and 'interface' are mutually exclusive; "
                                       "configure one listener binding");
                return false;
            }
            const QHostAddress address(value);
            if (address.isNull()) {
                error = QStringLiteral("invalid address: %1").arg(value);
                return false;
            }
            parsed.listenAddress = address;
            addressSeen = true;
            continue;
        }

        if (key == QStringLiteral("interface")) {
            if (addressSeen) {
                error = QStringLiteral("options 'address' and 'interface' are mutually exclusive; "
                                       "configure one listener binding");
                return false;
            }
            // Parsed and mapped only: resolving the interface is the shared runtime's decision, never this frontend's.
            parsed.listenInterface = value;
            interfaceSeen = true;
            continue;
        }

        if (key == QStringLiteral("port")) {
            bool ok = false;
            const int port = value.toInt(&ok);
            if (!ok || port < 1 || port > 65535) {
                error = QStringLiteral("port must be in the range 1..65535");
                return false;
            }
            parsed.port = static_cast<quint16>(port);
            continue;
        }

        if (key == QStringLiteral("input")) {
            bool enabled = false;
            if (!parseBoolean(value, enabled)) {
                error = QStringLiteral("input must be one of 0/1, false/true, off/on or no/yes");
                return false;
            }
            parsed.remoteInputEnabled = enabled;
            continue;
        }

        if (key == QStringLiteral("security")) {
            if (!parseSecurityProfile(value, parsed.securityProfile)) {
                error = QStringLiteral(
                    "security must be insecure, authenticated or authenticated-encrypted");
                return false;
            }
            continue;
        }

        if (key == QStringLiteral("security-config")) {
            parsed.securityConfigFile = value;
            continue;
        }

        error = QStringLiteral("unknown HyRemote Generic Plugin option: %1").arg(key);
        return false;
    }

    config = std::move(parsed);
    error.clear();
    return true;
}

}  // namespace HyRemote::Generic
