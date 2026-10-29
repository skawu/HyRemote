#include "host_paths.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>

namespace HyRemote::detail {

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
// a supplied-but-unusable value fails this one selection (never the caller's unrelated paths) instead of
// silently falling through to another writable location.
bool resolveOne(const QString &explicitValue, const char *environmentName,
                const QString &configRelativeDefault, const QString &platformDefault,
                HostPathSelection &selection, const QString &label,
                bool (*validate)(const QString &))
{
    if (!explicitValue.trimmed().isEmpty()) {
        const QString cleaned = QDir::cleanPath(QFileInfo(explicitValue).absoluteFilePath());
        if (validate && !validate(cleaned)) {
            selection.error = QStringLiteral("the explicit %1 '%2' is not usable").arg(label, cleaned);
            return false;
        }
        selection = {cleaned, HostPathOrigin::Explicit, {}};
        return true;
    }

    const QString fromEnvironment = environmentValue(environmentName);
    if (!fromEnvironment.isEmpty()) {
        const QString cleaned = QDir::cleanPath(QFileInfo(fromEnvironment).absoluteFilePath());
        if (validate && !validate(cleaned)) {
            selection.error = QStringLiteral("the %1 selected by %2 ('%3') is not usable")
                                  .arg(label, QString::fromLatin1(environmentName), cleaned);
            return false;
        }
        selection = {cleaned, HostPathOrigin::Environment, {}};
        return true;
    }

    if (!configRelativeDefault.isEmpty()) {
        const QString cleaned = QDir::cleanPath(configRelativeDefault);
        // A derived default is not a user selection: if it is not usable, fall through to the platform
        // default instead of failing a deployment that may not need this path at all.
        if (!validate || validate(cleaned)) {
            selection = {cleaned, HostPathOrigin::ConfigRelative, {}};
            return true;
        }
    }

    if (!platformDefault.isEmpty()) {
        // A platform default is a best-effort location, not a user selection: it is reported as-is so a caller
        // can surface it, while only explicit and environment selections fail closed.
        selection = {QDir::cleanPath(platformDefault), HostPathOrigin::PlatformDefault, {}};
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

    // A configuration file that cannot be used leaves nothing to anchor: the state directory and the secret
    // reference are intentionally not guessed from a half-known deployment, so the callers cannot half-start.
    if (!resolveOne(explicitConfigFile, kConfigEnv, QString(), QString(), paths.configFile,
                    QStringLiteral("configuration file"), &isReadableFile)) {
        return paths;
    }

    // A configuration-relative state directory is anchored to the configuration file's own directory, so a
    // deployment can move the pair together without touching the process working directory.
    QString configRelativeState;
    if (paths.configFile.isSet())
        configRelativeState = QFileInfo(paths.configFile.value).absolutePath() + QStringLiteral("/state");

    // The state directory and the secret reference are independent features: a broken state directory must
    // not hide a usable secret mount, so its failure is recorded on its own selection and resolution goes on.
    resolveOne(explicitStateDir, kStateEnv, configRelativeState,
               QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation),
               paths.stateDir, QStringLiteral("state directory"), &isUsableStateDirectory);
    resolveOne(explicitSecretFile, kSecretEnv, QString(), QString(), paths.secretFile,
               QStringLiteral("secret file"), &isReadableFile);

    return paths;
}

QString HostPaths::error() const
{
    QStringList failures;
    for (const HostPathSelection *selection : {&configFile, &stateDir, &secretFile}) {
        if (!selection->ok())
            failures << selection->error;
    }
    return failures.join(QStringLiteral("; "));
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

} // namespace HyRemote::detail
