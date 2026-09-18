// SPDX-License-Identifier: Apache-2.0
#include <cstddef>
#include <iostream>

#include "detail/input_mailbox_admission.hpp"

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

hyremote::InputEvent key(hyremote::KeyCode code, bool pressed)
{
    hyremote::InputEvent event;
    event.kind = hyremote::InputEventKind::Key;
    event.key = code;
    event.pressed = pressed;
    return event;
}

hyremote::InputEvent button(hyremote::PointerButton which, bool pressed)
{
    hyremote::InputEvent event;
    event.kind = hyremote::InputEventKind::PointerButton;
    event.button = which;
    event.pressed = pressed;
    return event;
}

hyremote::InputEvent text()
{
    hyremote::InputEvent event;
    event.kind = hyremote::InputEventKind::Text;
    event.textUtf8 = "x";
    return event;
}

void testNormalBudgetAndRejectedPressRelease()
{
    HyRemote::detail::InputMailboxAdmission admission;

    for (std::size_t i = 0; i < HyRemote::detail::InputMailboxAdmission::kNormalCapacity; ++i) {
        CHECK(admission.canAcceptNormal());
        admission.acceptNormal(text());
    }
    CHECK(!admission.canAcceptNormal());

    // A press rejected because the normal budget is full is never recorded as an accepted hold.
    // Its later protocol release is therefore harmless and must not consume protected capacity.
    const hyremote::InputEvent press = key(hyremote::KeyCode::A, true);
    const hyremote::InputEvent release = key(hyremote::KeyCode::A, false);
    CHECK(admission.classify(press) == HyRemote::detail::InputMailboxAdmission::Class::Normal);
    CHECK(admission.classify(release)
          == HyRemote::detail::InputMailboxAdmission::Class::DropUnmatchedRelease);

    for (int i = 0; i < 1000; ++i) {
        CHECK(admission.classify(press) == HyRemote::detail::InputMailboxAdmission::Class::Normal);
        CHECK(admission.classify(release)
              == HyRemote::detail::InputMailboxAdmission::Class::DropUnmatchedRelease);
    }
    CHECK(admission.protectedReleasePending() == 0U);
}

void testAcceptedHoldReleaseUsesProtectedReserve()
{
    HyRemote::detail::InputMailboxAdmission admission;

    const hyremote::InputEvent press = key(hyremote::KeyCode::A, true);
    const hyremote::InputEvent release = key(hyremote::KeyCode::A, false);
    admission.acceptNormal(press);
    admission.pendingBatchTaken();

    for (std::size_t i = 0; i < HyRemote::detail::InputMailboxAdmission::kNormalCapacity; ++i)
        admission.acceptNormal(text());
    CHECK(!admission.canAcceptNormal());

    CHECK(admission.classify(release)
          == HyRemote::detail::InputMailboxAdmission::Class::ProtectedRelease);
    CHECK(admission.canAcceptProtectedRelease());
    admission.acceptProtectedRelease(release);
    CHECK(admission.protectedReleasePending() == 1U);
    CHECK(admission.classify(release)
          == HyRemote::detail::InputMailboxAdmission::Class::DropUnmatchedRelease);
}

void testProtectedReserveBoundCoversWorstAcceptedLifecycle()
{
    using Admission = HyRemote::detail::InputMailboxAdmission;
    Admission admission;

    // Establish every supported logical held key. Split the presses across batches so the normal
    // pending budget is respected while accepted logical state persists after GUI draining.
    for (std::size_t index = 1; index < Admission::kKeySlotCount; ++index) {
        if (!admission.canAcceptNormal())
            admission.pendingBatchTaken();
        admission.acceptNormal(key(static_cast<hyremote::KeyCode>(index), true));
    }
    if (!admission.canAcceptNormal())
        admission.pendingBatchTaken();
    admission.acceptNormal(button(hyremote::PointerButton::Left, true));
    admission.acceptNormal(button(hyremote::PointerButton::Middle, true));
    admission.acceptNormal(button(hyremote::PointerButton::Right, true));
    admission.pendingBatchTaken();

    // Release all 72 pre-existing accepted holds into one stalled pending interval.
    for (std::size_t index = 1; index < Admission::kKeySlotCount; ++index) {
        const auto release = key(static_cast<hyremote::KeyCode>(index), false);
        CHECK(admission.classify(release) == Admission::Class::ProtectedRelease);
        CHECK(admission.canAcceptProtectedRelease());
        admission.acceptProtectedRelease(release);
    }
    for (hyremote::PointerButton which : {hyremote::PointerButton::Left,
                                         hyremote::PointerButton::Middle,
                                         hyremote::PointerButton::Right}) {
        const auto release = button(which, false);
        CHECK(admission.classify(release) == Admission::Class::ProtectedRelease);
        CHECK(admission.canAcceptProtectedRelease());
        admission.acceptProtectedRelease(release);
    }
    CHECK(admission.protectedReleasePending() == Admission::kMaxAcceptedHeldStates);

    // While those releases are pending, accept the maximum 64 new press lifecycles and their
    // matching releases. This reaches the exact proven reserve bound of 136 without overflow.
    for (std::size_t i = 0; i < Admission::kNormalCapacity; ++i) {
        const auto code = static_cast<hyremote::KeyCode>(1U + (i % Admission::kKnownKeyCount));
        const auto press = key(code, true);
        const auto release = key(code, false);
        CHECK(admission.canAcceptNormal());
        admission.acceptNormal(press);
        CHECK(admission.classify(release) == Admission::Class::ProtectedRelease);
        CHECK(admission.canAcceptProtectedRelease());
        admission.acceptProtectedRelease(release);
    }

    CHECK(admission.normalPending() == Admission::kNormalCapacity);
    CHECK(admission.protectedReleasePending() == Admission::kProtectedReleaseCapacity);
    CHECK(!admission.canAcceptNormal());
    CHECK(!admission.canAcceptProtectedRelease());
}

void testResetClearsAcceptedLifecycle()
{
    HyRemote::detail::InputMailboxAdmission admission;
    admission.acceptNormal(key(hyremote::KeyCode::Shift, true));
    admission.acceptNormal(button(hyremote::PointerButton::Left, true));
    admission.resetAll();

    CHECK(admission.normalPending() == 0U);
    CHECK(admission.protectedReleasePending() == 0U);
    CHECK(admission.classify(key(hyremote::KeyCode::Shift, false))
          == HyRemote::detail::InputMailboxAdmission::Class::DropUnmatchedRelease);
    CHECK(admission.classify(button(hyremote::PointerButton::Left, false))
          == HyRemote::detail::InputMailboxAdmission::Class::DropUnmatchedRelease);
}

}  // namespace

int main()
{
    testNormalBudgetAndRejectedPressRelease();
    testAcceptedHoldReleaseUsesProtectedReserve();
    testProtectedReserveBoundCoversWorstAcceptedLifecycle();
    testResetClearsAcceptedLifecycle();

    if (failures != 0)
        std::cerr << failures << " input mailbox admission checks failed\n";
    return failures == 0 ? 0 : 1;
}
