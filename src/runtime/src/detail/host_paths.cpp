#include "host_paths.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace hyremote::detail {

namespace {

constexpr auto kConfigEnv = "HYREMOTE_CONFIG_FILE";
constexpr auto kStateEnv = "HYREMOTE_STATE_DIR";
constexpr auto kSecretEnv = "HYREMOTE_SECRET_FILE";

QString environmentValue(const char *name)
{
    const QByteArray raw = qgetenv(name);
    return QString::fromUtf8(raw).trimmed();
}

bool isReadableFile(const QString &path)
{
    const QFileInfo info(path);
    return info.exists() && info.isFile() && info.isReadable();
}

bool isUsableStateDirectory(const QString &path)
{
    const QFileInfo info(path);
    if (info.exists())
        return info.isDir() && info.isWritable();
    const QFileInfo parent(info.absolutePath());
    return parent.exists() && parent.isDir() && parent.isWritable();
}

// Resolves one path through the documented precedence. `validate` returns true when the value is usable;
// a supplied-but-unusable value fails the whole resolution instead of silently falling through.
bool resolveOne(const QString &explicitValue, const char *environmentName,
                const QString &configRelativeDefault, const QString &platformDefault,
                HostPathSelection &selection, QString &error, const QString &label,
                bool (*validate)(const QString &))
{
    if (!explicitValue.trimmed().isEmpty()) {
        const QString cleaned = QDir::cleanPath(QFileInfo(explicitValue).absoluteFilePath());
        if (validate && !validate(cleaned)) {
            error = QStringLiteral("the explicit %1 '%2' is not usable").arg(label, cleaned);
            return false;
        }
        selection = {cleaned, HostPathOrigin::Explicit};
        return true;
    }

    const QString fromEnvironment = environmentValue(environmentName);
    if (!fromEnvironment.isEmpty()) {
        const QString cleaned = QDir::cleanPath(QFileInfo(fromEnvironment).absoluteFilePath());
        if (validate && !validate(cleaned)) {
            error = QStringLiteral("the %1 selected by %2 ('%3') is not usable")
                        .arg(label, QString::fromLatin1(environmentName), cleaned);
            return false;
        }
        selection = {cleaned, HostPathOrigin::Environment};
        return true;
    }

    if (!configRelativeDefault.isEmpty()) {
        const QString cleaned = QDir::cleanPath(configRelativeDefault);
        if (validate && !validate(cleaned)) {
            error = QStringLiteral("the configuration-relative %1 '%2' is not usable").arg(label, cleaned);
            return false;
        }
        selection = {cleaned, HostPathOrigin::ConfigRelative};
        return true;
    }

    if (!platformDefault.isEmpty()) {
        const QString cleaned = QDir::cleanPath(platformDefault);
        if (validate && !validate(cleaned)) {
            error = QStringLiteral("the platform default %1 '%2' is not usable").arg(label, cleaned);
            return false;
        }
        selection = {cleaned, HostPathOrigin::PlatformDefault};
        return true;
    }

    selection = {};
    return true;
}

} // namespace

HostPaths resolveHostPaths(const QString &explicitConfigFile, const QString &explicitStateDir,
                           const QString &explicitSecretFile)
{
    HostPaths paths;

    if (!resolveOne(explicitConfigFile, kConfigEnv, QString(), QString(), paths.configFile, paths.error,
                    QStringLiteral("configuration file"), &isReadableFile)) {
        return paths;
    }

    // A configuration-relative state directory is anchored to the configuration file's own directory, so a
    // deployment can move the pair together without touching the process working directory.
    QString configRelativeState;
    if (paths.configFile.isSet())
        configRelativeState = QFileInfo(paths.configFile.value).absolutePath() + QStringLiteral("/state");

    if (!resolveOne(explicitStateDir, kStateEnv, configRelativeState,
                    QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation),
                    paths.stateDir, paths.error, QStringLiteral("state directory"),
                    &isUsableStateDirectory)) {
        return paths;
    }

    if (!resolveOne(explicitSecretFile, kSecretEnv, QString(), QString(), paths.secretFile, paths.error,
                    QStringLiteral("secret file"), &isReadableFile)) {
        return paths;
    }

    return paths;
}

const char *hostPathOriginName(HostPathOrigin origin)
{
    switch (origin) {
    case HostPathOrigin::Explicit:
        return "explicit";
    case HostPathOrigin::Environment:
        return "environment";
    case HostPathOrigin::ConfigRelative:
        return "config-relative";
    case HostPathOrigin::PlatformDefault:
        return "platform-default";
    case HostPathOrigin::None:
        break;
    }
    return "none";
}

} // namespace hyremote::detail
