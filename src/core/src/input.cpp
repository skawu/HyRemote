#include "hyremote/core/input.hpp"

#include <algorithm>
#include <cmath>

namespace hyremote {

bool isValidInputViewport(const InputViewport &viewport) noexcept
{
    return viewport.width > 0U && viewport.height > 0U
        && std::isfinite(viewport.devicePixelRatio) && viewport.devicePixelRatio > 0.0F;
}

bool isPointerInputEvent(InputEventKind kind) noexcept
{
    return kind == InputEventKind::PointerMove || kind == InputEventKind::PointerButton
        || kind == InputEventKind::PointerScroll;
}

std::optional<MappedInputPoint> mapPointerToTarget(const InputEvent &event,
                                                   float targetWidth,
                                                   float targetHeight) noexcept
{
    if (!isPointerInputEvent(event.kind) || !isValidInputViewport(event.sourceViewport))
        return std::nullopt;
    if (!std::isfinite(targetWidth) || !std::isfinite(targetHeight) || targetWidth <= 0.0F
        || targetHeight <= 0.0F)
        return std::nullopt;
    if (!std::isfinite(event.x) || !std::isfinite(event.y))
        return std::nullopt;

    const float sourceMaxX = event.sourceViewport.width > 1U
        ? static_cast<float>(event.sourceViewport.width - 1U)
        : 0.0F;
    const float sourceMaxY = event.sourceViewport.height > 1U
        ? static_cast<float>(event.sourceViewport.height - 1U)
        : 0.0F;

    const float targetMaxX = targetWidth > 1.0F ? targetWidth - 1.0F : 0.0F;
    const float targetMaxY = targetHeight > 1.0F ? targetHeight - 1.0F : 0.0F;

    const float clampedX = std::clamp(event.x, 0.0F, sourceMaxX);
    const float clampedY = std::clamp(event.y, 0.0F, sourceMaxY);

    MappedInputPoint point;
    point.x = sourceMaxX > 0.0F ? (clampedX / sourceMaxX) * targetMaxX : 0.0F;
    point.y = sourceMaxY > 0.0F ? (clampedY / sourceMaxY) * targetMaxY : 0.0F;
    return point;
}

}  // namespace hyremote
