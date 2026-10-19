#pragma once

#include "composite_target.hpp"

#include <functional>

namespace HyRemote::Runtime::Automatic {

// Runtime-private composite specialization that adds normalized remote-input routing to the shared
// composite capture target used by Generic Plugin and QPA zero-code integration. A bounded explicit
// target may optionally refresh its Qt-owned surface membership at the same GUI-thread input boundary.
class InteractiveCompositeTarget final : public CompositeTarget
{
public:
    using SurfaceRefreshHandler = std::function<void()>;
    using CompositeTarget::CompositeTarget;

    void setSurfaceRefreshHandler(SurfaceRefreshHandler handler);
    void refreshSurfaces();

    ::HyRemote::detail::TargetComponents createTargetComponents(
        bool remoteInputEnabled,
        const ::HyRemote::detail::BuiltinTargetResolver &resolveBuiltinTarget) override;

private:
    SurfaceRefreshHandler m_surfaceRefreshHandler;
};

}  // namespace HyRemote::Runtime::Automatic
