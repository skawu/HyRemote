#include "automatic/automatic_access_controller.hpp"
#include "automatic/automatic_access_config.hpp"

#include <QDebug>
#include <QGenericPlugin>
#include <QHostAddress>
#include <QStringList>

#include <memory>
#include <utility>

namespace HyRemote::Generic {
namespace {

using Runtime::Automatic::AccessConfig;
using Runtime::Automatic::AccessController;
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

bool parseSpecification(const QString &specification, AccessConfig &config, QString &error)
{
    AccessConfig parsed;
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
            const QHostAddress address(value);
            if (address.isNull()) {
                error = QStringLiteral("invalid address: %1").arg(value);
                return false;
            }
            parsed.listenAddress = address;
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

class GenericRuntime final : public QObject
{
public:
    explicit GenericRuntime(AccessConfig config)
        : m_controller(std::make_unique<AccessController>(std::move(config)))
    {
    }

    bool start() { return m_controller && m_controller->start(); }

private:
    std::unique_ptr<AccessController> m_controller;
};

}  // namespace

class HyRemoteGenericPlugin final : public QGenericPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QGenericPluginFactoryInterface_iid FILE "hyremote.json")

public:
    QObject *create(const QString &key, const QString &specification) override
    {
        if (key.compare(QStringLiteral("hyremote"), Qt::CaseInsensitive) != 0)
            return nullptr;

        AccessConfig config;
        QString error;
        if (!parseSpecification(specification, config, error)) {
            qWarning() << "HyRemote Generic Plugin rejected its specification:" << error;
            return nullptr;
        }

        auto runtime = std::make_unique<GenericRuntime>(std::move(config));
        if (!runtime->start()) {
            qWarning() << "HyRemote Generic Plugin could not arm automatic access";
            return nullptr;
        }
        return runtime.release();
    }
};

}  // namespace HyRemote::Generic

#include "hyremote_generic_plugin.moc"
