// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "detail/component_factories.hpp"

namespace HyRemote::detail {

TargetComponents createWidgetsTargetComponents(QObject *target, bool remoteInputEnabled);

}  // namespace HyRemote::detail
