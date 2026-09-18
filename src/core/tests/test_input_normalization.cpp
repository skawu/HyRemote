// SPDX-License-Identifier: Apache-2.0
#include <cmath>
#include <limits>

#include "hyremote/core/input.hpp"
#include "test_support.hpp"

using namespace hyremote;
using namespace hyremote::test;

namespace {

bool near(float actual, float expected, float tolerance = 0.001F)
{
    return std::fabs(actual - expected) <= tolerance;
}

InputEvent pointerEvent(std::uint32_t width,
                        std::uint32_t height,
                        float x,
                        float y,
                        float dpr = 1.0F)
{
    InputEvent event;
    event.kind = InputEventKind::PointerMove;
    event.sourceViewport = InputViewport{width, height, dpr};
    event.x = x;
    event.y = y;
    return event;
}

}  // namespace

HYR_TEST(viewport_validation_rejects_missing_or_nonfinite_geometry)
{
    HYR_CHECK(isValidInputViewport(InputViewport{1U, 1U, 1.0F}));
    HYR_CHECK(!isValidInputViewport(InputViewport{0U, 1U, 1.0F}));
    HYR_CHECK(!isValidInputViewport(InputViewport{1U, 0U, 1.0F}));
    HYR_CHECK(!isValidInputViewport(InputViewport{1U, 1U, 0.0F}));
    HYR_CHECK(!isValidInputViewport(
        InputViewport{1U, 1U, std::numeric_limits<float>::infinity()}));
}

HYR_TEST(pointer_edges_map_edge_to_edge)
{
    InputEvent event = pointerEvent(640U, 480U, 639.0F, 479.0F);
    const auto mapped = mapPointerToTarget(event, 320.0F, 240.0F);
    HYR_CHECK(mapped.has_value());
    HYR_CHECK(near(mapped->x, 319.0F));
    HYR_CHECK(near(mapped->y, 239.0F));

    event.x = 0.0F;
    event.y = 0.0F;
    const auto origin = mapPointerToTarget(event, 320.0F, 240.0F);
    HYR_CHECK(origin.has_value());
    HYR_CHECK(near(origin->x, 0.0F));
    HYR_CHECK(near(origin->y, 0.0F));
}

HYR_TEST(device_pixel_source_maps_to_logical_target_without_double_dpr)
{
    // A 200x100 remote frame may represent a 100x50 logical target at DPR 2.0. Source dimensions
    // are authoritative, so the adapter passes the logical target size and DPR is not applied twice.
    InputEvent event = pointerEvent(200U, 100U, 199.0F, 99.0F, 2.0F);
    const auto mapped = mapPointerToTarget(event, 100.0F, 50.0F);
    HYR_CHECK(mapped.has_value());
    HYR_CHECK(near(mapped->x, 99.0F));
    HYR_CHECK(near(mapped->y, 49.0F));
}

HYR_TEST(out_of_range_pointer_coordinates_are_clamped)
{
    InputEvent event = pointerEvent(100U, 50U, -25.0F, 500.0F);
    const auto mapped = mapPointerToTarget(event, 100.0F, 50.0F);
    HYR_CHECK(mapped.has_value());
    HYR_CHECK(near(mapped->x, 0.0F));
    HYR_CHECK(near(mapped->y, 49.0F));
}

HYR_TEST(single_pixel_source_is_well_defined)
{
    InputEvent event = pointerEvent(1U, 1U, 0.0F, 0.0F);
    const auto mapped = mapPointerToTarget(event, 100.0F, 50.0F);
    HYR_CHECK(mapped.has_value());
    HYR_CHECK(near(mapped->x, 0.0F));
    HYR_CHECK(near(mapped->y, 0.0F));
}

HYR_TEST(non_pointer_or_invalid_target_mapping_is_rejected)
{
    InputEvent key;
    key.kind = InputEventKind::Key;
    key.key = KeyCode::Enter;
    HYR_CHECK(!mapPointerToTarget(key, 100.0F, 100.0F).has_value());

    InputEvent pointer = pointerEvent(100U, 100U, 10.0F, 10.0F);
    HYR_CHECK(!mapPointerToTarget(pointer, 0.0F, 100.0F).has_value());
    pointer.x = std::numeric_limits<float>::quiet_NaN();
    HYR_CHECK(!mapPointerToTarget(pointer, 100.0F, 100.0F).has_value());
}

HYR_TEST(modifier_mask_is_backend_neutral_and_composable)
{
    const InputModifiers modifiers = modifierMask(InputModifier::Shift)
        | modifierMask(InputModifier::Control) | modifierMask(InputModifier::Alt);
    HYR_CHECK(hasModifier(modifiers, InputModifier::Shift));
    HYR_CHECK(hasModifier(modifiers, InputModifier::Control));
    HYR_CHECK(hasModifier(modifiers, InputModifier::Alt));
    HYR_CHECK(!hasModifier(modifiers, InputModifier::Meta));
}

HYR_TEST(modifier_keys_have_explicit_press_release_identity)
{
    InputEvent shiftDown;
    shiftDown.kind = InputEventKind::Key;
    shiftDown.key = KeyCode::Shift;
    shiftDown.pressed = true;
    shiftDown.modifiers = modifierMask(InputModifier::Shift);
    HYR_CHECK(shiftDown.key == KeyCode::Shift);
    HYR_CHECK(shiftDown.pressed);
    HYR_CHECK(hasModifier(shiftDown.modifiers, InputModifier::Shift));

    InputEvent shiftUp = shiftDown;
    shiftUp.pressed = false;
    shiftUp.modifiers = 0U;
    HYR_CHECK(shiftUp.key == KeyCode::Shift);
    HYR_CHECK(!shiftUp.pressed);
    HYR_CHECK(!hasModifier(shiftUp.modifiers, InputModifier::Shift));

    HYR_CHECK(KeyCode::Control != KeyCode::Alt);
    HYR_CHECK(KeyCode::Meta != KeyCode::CapsLock);
    HYR_CHECK(KeyCode::CapsLock != KeyCode::NumLock);
}

HYR_TEST(key_text_and_scroll_have_separate_semantics)
{
    InputEvent key;
    key.kind = InputEventKind::Key;
    key.key = KeyCode::Backspace;
    key.pressed = true;
    HYR_CHECK(key.textUtf8.empty());

    InputEvent text;
    text.kind = InputEventKind::Text;
    text.textUtf8 = "A";
    HYR_CHECK(text.key == KeyCode::Unknown);

    InputEvent scroll;
    scroll.kind = InputEventKind::PointerScroll;
    scroll.sourceViewport = InputViewport{800U, 600U, 1.0F};
    scroll.x = 400.0F;
    scroll.y = 300.0F;
    scroll.scrollY = -1.0F;
    HYR_CHECK_EQ(scroll.scrollY, -1.0F);
    HYR_CHECK(isPointerInputEvent(scroll.kind));
}

HYR_TEST_MAIN()
