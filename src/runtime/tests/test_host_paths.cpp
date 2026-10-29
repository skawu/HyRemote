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
        const auto paths = HyRemote::detail::resolveHostPaths();
        check(paths.ok(), QStringLiteral("empty selection resolves"));
        check(!paths.configFile.isSet(), QStringLiteral("no configuration file is implied"));
        check(!paths.secretFile.isSet(), QStringLiteral("no secret file is implied"));
        check(paths.stateDir.isSet()
                  && paths.stateDir.origin == HyRemote::detail::HostPathOrigin::PlatformDefault,
              QStringLiteral("state directory falls back to the platform default"));
    }

    // 2. Explicit selection wins over the environment.
    {
        qputenv("HYREMOTE_CONFIG_FILE", QDir(root).absoluteFilePath(QStringLiteral("other.conf")).toUtf8());
        writeFile(root, QStringLiteral("other.conf"));
        const auto paths = HyRemote::detail::resolveHostPaths(configPath);
        check(paths.ok(), QStringLiteral("explicit configuration resolves"));
        check(paths.configFile.value == QFileInfo(configPath).absoluteFilePath(),
              QStringLiteral("explicit configuration wins over the environment"));
        check(paths.configFile.origin == HyRemote::detail::HostPathOrigin::Explicit,
              QStringLiteral("explicit origin recorded"));
        clearEnvironment();
    }

    // 3. Environment selection is used when no explicit value exists.
    {
        qputenv("HYREMOTE_CONFIG_FILE", configPath.toUtf8());
        const auto paths = HyRemote::detail::resolveHostPaths();
        check(paths.ok() && paths.configFile.origin == HyRemote::detail::HostPathOrigin::Environment,
              QStringLiteral("environment configuration is honoured"));
        clearEnvironment();
    }

    // 4. An explicit path that cannot be used fails closed instead of falling back to the environment.
    {
        qputenv("HYREMOTE_CONFIG_FILE", configPath.toUtf8());
        const auto paths = HyRemote::detail::resolveHostPaths(
            root.absoluteFilePath(QStringLiteral("missing.conf")));
        check(!paths.ok(), QStringLiteral("missing explicit configuration fails closed"));
        check(paths.error().contains(QStringLiteral("explicit")),
              QStringLiteral("failure names the explicit selection"));
        check(!paths.stateDir.isSet(),
              QStringLiteral("no state directory is resolved after a failed explicit selection"));
        clearEnvironment();
    }

    // 5. A configuration-relative state directory is anchored to the configuration file, not the CWD.
    {
        const auto paths = HyRemote::detail::resolveHostPaths(configPath);
        check(paths.ok(), QStringLiteral("configuration with relative state resolves"));
        check(paths.stateDir.origin == HyRemote::detail::HostPathOrigin::ConfigRelative,
              QStringLiteral("state directory is anchored to the configuration file"));
        check(paths.stateDir.value == QDir(root.absoluteFilePath(QStringLiteral("state"))).absolutePath(),
              QStringLiteral("state directory sits next to the configuration file"));
    }

    // 6. A state directory whose parent does not exist fails closed.
    {
        const auto paths = HyRemote::detail::resolveHostPaths(
            configPath, root.absoluteFilePath(QStringLiteral("absent/state")));
        check(!paths.ok(), QStringLiteral("unusable explicit state directory fails closed"));
    }

    // 7. Explicit secret references are validated like configuration files.
    {
        const auto paths = HyRemote::detail::resolveHostPaths(configPath, QString(), secretPath);
        check(paths.ok() && paths.secretFile.origin == HyRemote::detail::HostPathOrigin::Explicit,
              QStringLiteral("explicit secret file resolves"));

        const auto broken = HyRemote::detail::resolveHostPaths(
            configPath, QString(), root.absoluteFilePath(QStringLiteral("no-secret.txt")));
        check(!broken.ok(), QStringLiteral("missing explicit secret file fails closed"));
    }

    // 8. #591 dependency scoping: a failure belongs to the path it names. An unusable state directory must
    // not take the readable configuration and the readable secret reference down with it, and the aggregate
    // message must not blame a path that resolved.
    {
        const auto paths = HyRemote::detail::resolveHostPaths(
            configPath, root.absoluteFilePath(QStringLiteral("absent/state")), secretPath);
        check(!paths.ok(), QStringLiteral("unusable state directory is reported"));
        check(!paths.stateDir.ok() && paths.stateDir.error.contains(QStringLiteral("state directory")),
              QStringLiteral("the failure is attributed to the state directory"));
        check(paths.configFile.ok() && paths.configFile.isSet(),
              QStringLiteral("a readable configuration stays usable"));
        check(paths.secretFile.ok() && paths.secretFile.isSet(),
              QStringLiteral("a readable secret reference stays usable"));
        check(paths.error().contains(QStringLiteral("state directory"))
                  && !paths.error().contains(QStringLiteral("secret file")),
              QStringLiteral("the aggregate message names only the failing path"));
    }

    // 9. An unusable secret reference is equally scoped: configuration and state stay resolved.
    {
        const auto paths = HyRemote::detail::resolveHostPaths(
            configPath, QString(), root.absoluteFilePath(QStringLiteral("no-secret.txt")));
        check(!paths.ok() && !paths.secretFile.ok(), QStringLiteral("unusable secret reference is reported"));
        check(paths.configFile.ok() && paths.configFile.isSet(),
              QStringLiteral("configuration is unaffected by a broken secret reference"));
        check(paths.stateDir.ok() && paths.stateDir.isSet(),
              QStringLiteral("state directory is unaffected by a broken secret reference"));
    }

    // 10. #591 Windows acceptance: UTF-8 and space-bearing directories/files stay one usable selection and
    // are reported as the same absolute location (no encoding round trip through the local 8-bit codec).
    {
        const QString directoryName = QStringLiteral("水印 tests");
        const QString fileName = QStringLiteral("描述 file.conf");
        check(root.mkdir(directoryName), QStringLiteral("UTF-8/space directory is created"));
        const QString utf8Directory = root.absoluteFilePath(directoryName);
        const QString utf8Path = writeFile(QDir(utf8Directory), fileName);
        check(!utf8Path.isEmpty(), QStringLiteral("UTF-8/space descriptor fixture exists"));

        const auto paths = HyRemote::detail::resolveHostPaths(utf8Path);
        check(paths.configFile.ok() && paths.configFile.isSet(),
              QStringLiteral("UTF-8/space descriptor resolves"));
        check(paths.configFile.value == QFileInfo(utf8Path).absoluteFilePath(),
              QStringLiteral("UTF-8/space location is reported unchanged"));
        check(paths.configFile.value.contains(directoryName) && paths.configFile.value.contains(fileName),
              QStringLiteral("UTF-8/space path components survive normalization"));
        check(paths.stateDir.origin == HyRemote::detail::HostPathOrigin::ConfigRelative
                  && paths.stateDir.value.startsWith(utf8Directory),
              QStringLiteral("UTF-8/space config-relative state directory stays anchored"));
    }

    // 11. A relative explicit selection is normalized against the process directory once and reported as an
    // absolute location, so a later working-directory change cannot move the same configuration.
    {
        const QString previous = QDir::currentPath();
        check(QDir::setCurrent(root.path()), QStringLiteral("test switches into the fixture directory"));
        const auto paths = HyRemote::detail::resolveHostPaths(QStringLiteral("security.conf"));
        check(QDir::setCurrent(previous), QStringLiteral("test restores the previous directory"));

        check(paths.configFile.ok() && paths.configFile.origin == HyRemote::detail::HostPathOrigin::Explicit,
              QStringLiteral("relative explicit selection resolves as an explicit selection"));
        check(paths.configFile.value == QFileInfo(root.absoluteFilePath(QStringLiteral("security.conf")))
                                            .absoluteFilePath(),
              QStringLiteral("relative selection is reported as an absolute location"));
    }

    // 12. A location that exists but is not a readable file is still unusable: existence alone is not
    // evidence that a descriptor can be loaded.
    {
        const auto paths = HyRemote::detail::resolveHostPaths(root.path());
        check(!paths.ok(), QStringLiteral("a directory offered as a descriptor is refused"));
        check(!paths.configFile.ok() && paths.configFile.error.contains(QStringLiteral("not usable")),
              QStringLiteral("the directory is reported as unusable"));
        check(!paths.configFile.isSet(), QStringLiteral("no descriptor location is claimed"));
        check(!paths.stateDir.isSet() && !paths.secretFile.isSet(),
              QStringLiteral("a refused descriptor resolves no dependent path"));
    }

    check(HyRemote::detail::hostPathOriginName(HyRemote::detail::HostPathOrigin::ConfigRelative)
              == QByteArray("config-relative"),
          QStringLiteral("origin names are stable"));

    QTextStream out(stdout);
    out << (failures == 0 ? "host path checks passed\n" : "host path checks failed\n");
    return failures == 0 ? 0 : 2;
}
