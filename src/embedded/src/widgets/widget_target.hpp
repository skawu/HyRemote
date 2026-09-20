#pragma once

#include "detail/component_factories.hpp"

namespace HyRemote::detail {

TargetComponents createWidgetsTargetComponents(QObject *target, bool remoteInputEnabled);

}  // namespace HyRemote::detail
