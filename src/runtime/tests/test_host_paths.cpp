// #591: the host-path contract is the shared resolution every integration route uses. These cases pin the
// precedence (explicit > environment > config-relative > platform default) and the fail-closed rule: a
// supplied-but-unusable selection must never silently fall back to another writable directory.

#include "../src/detail/host_paths.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

int failures = 0;

void check(bool condition, const QString &message)
{
    QTextStream out(stdout);
    if (condition) {
        out << "ok: " << message << '\n';
        return;
    }
    out << "FAIL: " << message << '\n';
    ++failures;
}

QString writeFile(const QDir &dir, const QString &name, const QByteArray &content = "fixture\n")
{
    const QString path = dir.absoluteFilePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return QString();
    file.write(content);
    file.close();
    return path;
}

void clearEnvironment()
{
    qunsetenv("HYREMOTE_CONFIG_FILE");
    qunsetenv("HYREMOTE_STATE_DIR");
    qunsetenv("HYREMOTE_SECRET_FILE");
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    clearEnvironment();

    QTemporaryDir workspace;
    check(workspace.isValid(), QStringLiteral("temporary workspace"));
    if (!workspace.isValid())
        return 1;

    QDir root(workspace.path());
    check(root.mkdir(QStringLiteral("state")), QStringLiteral("state directory created"));
    const QString configPath = writeFile(root, QStringLiteral("security.conf"),
                                         "version=1\ncredentialId=main\npasswordFile=secret.txt\n");
    const QString secretPath = writeFile(root, QStringLiteral("secret.txt"), "secret\n");
    check(!configPath.isEmpty() && !secretPath.isEmpty(), QStringLiteral("configuration fixtures"));

    // 1. Nothing selected: a platform default state directory, no implied configuration or secret file.
    {
        const auto paths = hyremote::detail::resolveHostPaths();
        check(paths.ok(), QStringLiteral("empty selection resolves"));
        check(!paths.configFile.isSet(), QStringLiteral("no configuration file is implied"));
        check(!paths.secretFile.isSet(), QStringLiteral("no secret file is implied"));
        check(paths.stateDir.isSet()
                  && paths.stateDir.origin == hyremote::detail::HostPathOrigin::PlatformDefault,
              QStringLiteral("state directory falls back to the platform default"));
    }

    // 2. Explicit selection wins over the environment.
    {
        qputenv("HYREMOTE_CONFIG_FILE", QDir(root).absoluteFilePath(QStringLiteral("other.conf")).toUtf8());
        writeFile(root, QStringLiteral("other.conf"));
        const auto paths = hyremote::detail::resolveHostPaths(configPath);
        check(paths.ok(), QStringLiteral("explicit configuration resolves"));
        check(paths.configFile.value == QFileInfo(configPath).absoluteFilePath(),
              QStringLiteral("explicit configuration wins over the environment"));
        check(paths.configFile.origin == hyremote::detail::HostPathOrigin::Explicit,
              QStringLiteral("explicit origin recorded"));
        clearEnvironment();
    }

    // 3. Environment selection is used when no explicit value exists.
    {
        qputenv("HYREMOTE_CONFIG_FILE", configPath.toUtf8());
        const auto paths = hyremote::detail::resolveHostPaths();
        check(paths.ok() && paths.configFile.origin == hyremote::detail::HostPathOrigin::Environment,
              QStringLiteral("environment configuration is honoured"));
        clearEnvironment();
    }

    // 4. An explicit path that cannot be used fails closed instead of falling back to the environment.
    {
        qputenv("HYREMOTE_CONFIG_FILE", configPath.toUtf8());
        const auto paths = hyremote::detail::resolveHostPaths(
            root.absoluteFilePath(QStringLiteral("missing.conf")));
        check(!paths.ok(), QStringLiteral("missing explicit configuration fails closed"));
        check(paths.error.contains(QStringLiteral("explicit")),
              QStringLiteral("failure names the explicit selection"));
        check(!paths.stateDir.isSet(),
              QStringLiteral("no state directory is resolved after a failed explicit selection"));
        clearEnvironment();
    }

    // 5. A configuration-relative state directory is anchored to the configuration file, not the CWD.
    {
        const auto paths = hyremote::detail::resolveHostPaths(configPath);
        check(paths.ok(), QStringLiteral("configuration with relative state resolves"));
        check(paths.stateDir.origin == hyremote::detail::HostPathOrigin::ConfigRelative,
              QStringLiteral("state directory is anchored to the configuration file"));
        check(paths.stateDir.value == QDir(root.absoluteFilePath(QStringLiteral("state"))).absolutePath(),
              QStringLiteral("state directory sits next to the configuration file"));
    }

    // 6. A state directory whose parent does not exist fails closed.
    {
        const auto paths = hyremote::detail::resolveHostPaths(
            configPath, root.absoluteFilePath(QStringLiteral("absent/state")));
        check(!paths.ok(), QStringLiteral("unusable explicit state directory fails closed"));
    }

    // 7. Explicit secret references are validated like configuration files.
    {
        const auto paths = hyremote::detail::resolveHostPaths(configPath, QString(), secretPath);
        check(paths.ok() && paths.secretFile.origin == hyremote::detail::HostPathOrigin::Explicit,
              QStringLiteral("explicit secret file resolves"));

        const auto broken = hyremote::detail::resolveHostPaths(
            configPath, QString(), root.absoluteFilePath(QStringLiteral("no-secret.txt")));
        check(!broken.ok(), QStringLiteral("missing explicit secret file fails closed"));
    }

    check(hyremote::detail::hostPathOriginName(hyremote::detail::HostPathOrigin::ConfigRelative)
              == QByteArray("config-relative"),
          QStringLiteral("origin names are stable"));

    QTextStream out(stdout);
    out << (failures == 0 ? "host path checks passed\n" : "host path checks failed\n");
    return failures == 0 ? 0 : 2;
}
