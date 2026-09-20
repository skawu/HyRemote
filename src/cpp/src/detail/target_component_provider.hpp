#pragma once

#include <functional>

#include "detail/component_factories.hpp"

class QObject;

namespace HyRemote::detail {

using BuiltinTargetResolver =
    std::function<TargetComponents(QObject *target, bool remoteInputEnabled)>;

// Private, per-target composition seam used by product-internal adapters such as the
// QPA multi-surface target. It is deliberately not part of the installed
// HyRemote public API.
//
// Unlike setTargetFactory(), this interface is scoped to one QObject instance and therefore
// cannot globally replace target-adapter semantics for other RemoteAccess instances in the
// process. The resolver passed by RemoteAccess bypasses providers and reaches only the normal
// built-in Widgets/Quick adapter chain, allowing a composite target to reuse those adapters
// without recursion or duplicated capture/input semantics.
class TargetComponentProvider
{
public:
    virtual ~TargetComponentProvider() = default;

    virtual TargetComponents createTargetComponents(
        bool remoteInputEnabled,
        const BuiltinTargetResolver &resolveBuiltinTarget) = 0;
};

}  // namespace HyRemote::detail
