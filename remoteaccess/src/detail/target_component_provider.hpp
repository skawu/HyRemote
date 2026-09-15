#pragma once

#include <QtPlugin>

#include <functional>

#include "detail/component_factories.hpp"

class QObject;

namespace HyRemote::detail {

using BuiltinTargetResolver =
    std::function<TargetComponents(QObject *target, bool remoteInputEnabled)>;

// Private, per-target composition seam used by product-internal adapters such as the
// Transparent QPA multi-surface target. It is deliberately not part of the installed
// HyRemote public API.
//
// This is a Qt plugin-style interface rather than an RTTI-only base. QPA is a MODULE and
// RemoteAccess may be a shared library on Windows; Q_DECLARE_INTERFACE/Q_INTERFACES lets
// qobject_cast identify the provider across that dynamic-library boundary without relying on
// unexported C++ RTTI. The interface remains private and version-coupled to the HyRemote build.
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

Q_DECLARE_INTERFACE(HyRemote::detail::TargetComponentProvider,
                    "org.hyremote.internal.TargetComponentProvider/1.0")
