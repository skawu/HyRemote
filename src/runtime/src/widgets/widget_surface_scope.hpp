#pragma once

#include "detail/component_factories.hpp"

namespace HyRemote::detail {

// Builds one explicit QWidget target as a bounded application-surface scope: the configured
// widget plus only visible top-level surfaces that Qt can prove are owned/transient descendants.
// This is Runtime-private and does not change the public target API.
TargetComponents createScopedWidgetsTargetComponents(QObject *target, bool remoteInputEnabled);

}  // namespace HyRemote::detail
