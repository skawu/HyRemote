#include <QCoreApplication>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QStringList>
#include <QTimer>
#include <QVariant>

#ifdef __linux__
#include <link.h>

#include <filesystem>
#include <string>
#endif

namespace {

#ifdef __linux__
struct LoadedLibraryCheck
{
    std::filesystem::path prefix;
    bool sawRemoteAccess = false;
    bool sawQmlBacking = false;
    bool sawQt = false;
    bool escapedPrefix = false;
};

bool isWithin(const std::filesystem::path &path, const std::filesystem::path &prefix)
{
    const std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(path);
    const std::filesystem::path normalizedPrefix = std::filesystem::weakly_canonical(prefix);

    auto pathIt = normalizedPath.begin();
    auto prefixIt = normalizedPrefix.begin();
    for (; prefixIt != normalizedPrefix.end(); ++prefixIt, ++pathIt) {
        if (pathIt == normalizedPath.end() || *pathIt != *prefixIt)
            return false;
    }
    return true;
}

int inspectLoadedLibrary(dl_phdr_info *info, std::size_t, void *opaque)
{
    auto &check = *static_cast<LoadedLibraryCheck *>(opaque);
    if (!info || !info->dlpi_name || info->dlpi_name[0] == '\0')
        return 0;

    const std::filesystem::path libraryPath(info->dlpi_name);
    const std::string filename = libraryPath.filename().string();
    const bool isRemoteAccess = filename.rfind("libHyRemoteRemoteAccess.so", 0) == 0;
    const bool isQmlBacking = filename.rfind("libhyremote-qml.so", 0) == 0;
    const bool isQpa = filename.rfind("libqhyremote.so", 0) == 0;
    const bool isQt = filename.rfind("libQt6", 0) == 0;
    if (!isRemoteAccess && !isQmlBacking && !isQpa && !isQt)
        return 0;

    check.sawRemoteAccess = check.sawRemoteAccess || isRemoteAccess;
    check.sawQmlBacking = check.sawQmlBacking || isQmlBacking;
    check.sawQt = check.sawQt || isQt;
    if (!isWithin(libraryPath, check.prefix))
        check.escapedPrefix = true;
    return 0;
}

bool loadedProductLibrariesComeFromDeployment()
{
    const std::filesystem::path executable = std::filesystem::canonical("/proc/self/exe");
    const std::filesystem::path prefix = executable.parent_path().parent_path();

    LoadedLibraryCheck check{prefix};
    dl_iterate_phdr(&inspectLoadedLibrary, &check);
    return check.sawRemoteAccess && check.sawQmlBacking && check.sawQt && !check.escapedPrefix;
}
#endif

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    int testSeconds = 0;
    const QStringList arguments = QCoreApplication::arguments();
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        if (arguments.at(index) != QStringLiteral("--test-seconds"))
            continue;
        if (index + 1 >= arguments.size())
            return 4;
        bool ok = false;
        testSeconds = arguments.at(++index).toInt(&ok);
        if (!ok || testSeconds <= 0 || testSeconds > 60)
            return 4;
    }

    QQmlApplicationEngine engine;
    engine.loadFromModule("HyRemoteInstalledConsumer", "Main");
    if (engine.rootObjects().isEmpty())
        return 2;

    QObject *root = engine.rootObjects().constFirst();
    if (!root || !root->property("contractOk").toBool())
        return 3;

#ifdef __linux__
    // Import success alone is insufficient clean-deployment evidence while the original Qt/HyRemote
    // trees still exist on a CI worker. After QML has actually loaded its module, require the shared
    // facade, declarative backing runtime, every loaded Qt6 library, and qhyremote when present to
    // resolve from this application's own deployment prefix.
    if (!loadedProductLibrariesComeFromDeployment())
        return 5;
#endif

    // Normal clean-QML consumption only needs to prove import/instantiation/deployment and exits
    // immediately. The combined QML+QPA configuration asks the exact same application to stay alive
    // long enough for the transparent platform runtime to accept/reaccept RFB viewers.
    if (testSeconds == 0)
        return 0;

    QTimer::singleShot(testSeconds * 1000, &app, &QCoreApplication::quit);
    return app.exec();
}
