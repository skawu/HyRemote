#include <HyRemote/RemoteAccess.h>

#include <QObject>

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
    bool sawHyRemote = false;
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
    const bool isHyRemote = filename.rfind("libHyRemoteRemoteAccess.so", 0) == 0;
    const bool isQt = filename.rfind("libQt6", 0) == 0;
    if (!isHyRemote && !isQt)
        return 0;

    check.sawHyRemote = check.sawHyRemote || isHyRemote;
    check.sawQt = check.sawQt || isQt;
    if (!isWithin(libraryPath, check.prefix))
        check.escapedPrefix = true;
    return 0;
}

bool loadedProductLibrariesComeFromDeployment()
{
    // The canonical source-consumer fixture installs the executable in <prefix>/bin. Resolve the
    // actual process image rather than argv[0] so symlink/wrapper invocation cannot weaken the
    // check. All HyRemote and Qt6 shared objects used by this clean deployment must resolve below
    // that same prefix, not from the source build or the original Qt SDK.
    const std::filesystem::path executable = std::filesystem::canonical("/proc/self/exe");
    const std::filesystem::path prefix = executable.parent_path().parent_path();

    LoadedLibraryCheck check{prefix};
    dl_iterate_phdr(&inspectLoadedLibrary, &check);
    return check.sawHyRemote && check.sawQt && !check.escapedPrefix;
}
#endif

}  // namespace

int main()
{
    QObject target;
    HyRemote::RemoteAccess remote(&target);
    if (remote.target() != &target || remote.state() != HyRemote::RemoteAccessState::Stopped)
        return 1;

#ifdef __linux__
    if (!loadedProductLibrariesComeFromDeployment())
        return 2;
#endif

    return 0;
}
