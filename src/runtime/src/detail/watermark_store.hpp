#pragma once

// #591: the operator's explicitly configured watermark policy must survive a restart. The state directory is the place
// the host-path contract already reserves for mutable state, so the policy is written there and read back on the next
// start when this process made no explicit selection of its own.
//
// Two rules keep this from becoming a second configuration system:
//   * only the resolved *policy* is stored (text/position/opacity/size/colour) - never credentials, never descriptor
//     contents, never a password;
//   * a stored file is re-validated through the one shared policy resolver on load, so a hand-edited or stale file can
//     never bypass the bounds or smuggle an unknown position into the running product.

#include "host_paths.hpp"
#include "watermark_policy.hpp"

#include <QString>

#include <optional>

namespace HyRemote::detail {

// Empty when no state directory resolved, i.e. persistence is unavailable rather than silently in-memory.
QString watermarkStorePath(const HostPaths &paths);

bool saveWatermarkPolicy(const HostPaths &paths, const WatermarkPolicy &policy);

// Removes the stored policy. Absence of a file is success: the operator asked for "no watermark".
bool clearWatermarkPolicy(const HostPaths &paths);

// Returns the stored request when a usable one exists. An unreadable, malformed or out-of-bounds file yields no
// request and fills `error`, so the caller can report why persistence did not apply instead of silently pretending it
// did.
std::optional<WatermarkPolicyRequest> loadWatermarkRequest(const HostPaths &paths, QString *error);

}  // namespace HyRemote::detail
