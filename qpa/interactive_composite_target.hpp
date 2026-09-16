#pragma once

#include "composite_target.hpp"

namespace HyRemote::Qpa {

// QPA-04 specialization that adds normalized remote-input routing to the existing composite
// capture target. It remains private to the platform plugin and does not alter the installed
// RemoteAccess target API.
class InteractiveCompositeTarget final : public CompositeTarget
{
public:
    using CompositeTarget::CompositeTarget;

    ::HyRemote::detail::TargetComponents createTargetComponents(
        bool remoteInputEnabled,
        const ::HyRemote::detail::BuiltinTargetResolver &resolveBuiltinTarget) override;
};

}  // namespace HyRemote::Qpa
