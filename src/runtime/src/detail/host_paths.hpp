#pragma once

#include <QString>

namespace HyRemote::detail {

// #591: one typed host-path contract shared by every integration route. The route (C++ API, QML API,
// Generic plugin or QPA) only supplies its explicit selection; it never resolves paths on its own.
enum class HostPathOrigin {
    None,
    Explicit,
    Environment,
    ConfigRelative,
    PlatformDefault,
};

struct HostPathSelection {
    QString value;
    HostPathOrigin origin = HostPathOrigin::None;

    bool isSet() const { return !value.isEmpty(); }
};

struct HostPaths {
    HostPathSelection configFile;
    HostPathSelection stateDir;
    HostPathSelection secretFile;

    // Non-empty when an explicit or environment selection was given but is unusable. In that case the
    // caller must fail the dependent feature with this message and must not fall back to another
    // writable directory.
    QString error;

    bool ok() const { return error.isEmpty(); }
};

// Precedence per path: explicit argument > dedicated environment variable > config-file-relative
// default (anchored to the configuration file's directory, never the process working directory) >
// safe platform default for local non-container use.
//
// * HYREMOTE_CONFIG_FILE / HYREMOTE_STATE_DIR / HYREMOTE_SECRET_FILE are the environment selectors.
// * A configuration file and a secret file must already be readable files; a state directory must be
//   an existing writable directory or a path whose parent exists.
// * There is deliberately no working-directory or home-directory fallback.
HostPaths resolveHostPaths(const QString &explicitConfigFile = QString(),
                           const QString &explicitStateDir = QString(),
                           const QString &explicitSecretFile = QString());

const char *hostPathOriginName(HostPathOrigin origin);

} // namespace HyRemote::detail
