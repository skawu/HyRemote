#pragma once

#include "composite_target.hpp"

namespace HyRemote::Runtime::Automatic {

// Runtime-private composite specialization that adds normalized remote-input routing to the shared
// composite capture target used by Generic Plugin and QPA zero-code integration.
class InteractiveCompositeTarget final : public CompositeTarget
{
public:
    using CompositeTarget::CompositeTarget;

    ::HyRemote::detail::TargetComponents createTargetComponents(
        bool remoteInputEnabled,
        const ::HyRemote::detail::BuiltinTargetResolver &resolveBuiltinTarget) override;
};

}  // namespace HyRemote::Runtime::Automatic
