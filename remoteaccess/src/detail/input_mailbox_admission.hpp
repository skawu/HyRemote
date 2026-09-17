#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "hyremote/core/input.hpp"

namespace HyRemote::detail {

// Thread-side admission bookkeeping shared by the Widgets and Quick input adapters.
//
// Normal input keeps the historical 64-event pending budget. Lifecycle releases receive a separate
// bounded reserve, but only when this adapter previously accepted the matching logical press. A
// release for a press rejected by normal backpressure is therefore dropped rather than consuming
// the protected reserve or synthesizing an unmatched Qt release.
//
// The release reserve is strictly bounded. At the start of a pending interval there can be at most
// one accepted hold for each supported logical key plus three pointer buttons (69 + 3 = 72). During
// that interval at most kNormalCapacity additional presses can be accepted. Therefore 72 + 64 = 136
// protected releases is sufficient even when the GUI thread is stalled, while the mailbox remains
// bounded under hostile press/release traffic.
class InputMailboxAdmission final
{
public:
    enum class Class {
        Normal,
        ProtectedRelease,
        DropUnmatchedRelease,
    };

    static constexpr std::size_t kNormalCapacity = 64;
    static constexpr std::size_t kKeySlotCount =
        static_cast<std::size_t>(hyremote::KeyCode::F12) + 1U;
    static constexpr std::size_t kKnownKeyCount = kKeySlotCount - 1U;  // excludes Unknown
    static constexpr std::size_t kButtonCount = 3;
    static constexpr std::size_t kMaxAcceptedHeldStates = kKnownKeyCount + kButtonCount;
    static constexpr std::size_t kProtectedReleaseCapacity =
        kMaxAcceptedHeldStates + kNormalCapacity;

    Class classify(const hyremote::InputEvent &event) const noexcept
    {
        if (isKnownKeyRelease(event))
            return m_acceptedKeys[keyIndex(event.key)] ? Class::ProtectedRelease
                                                       : Class::DropUnmatchedRelease;

        if (const auto index = buttonIndexForRelease(event); index < kButtonCount)
            return m_acceptedButtons[index] ? Class::ProtectedRelease
                                            : Class::DropUnmatchedRelease;

        return Class::Normal;
    }

    bool canAcceptNormal() const noexcept { return m_normalPending < kNormalCapacity; }

    bool canAcceptProtectedRelease() const noexcept
    {
        return m_protectedReleasePending < kProtectedReleaseCapacity;
    }

    void acceptNormal(const hyremote::InputEvent &event) noexcept
    {
        ++m_normalPending;

        if (isKnownKeyPress(event)) {
            m_acceptedKeys[keyIndex(event.key)] = true;
            return;
        }

        if (const auto index = buttonIndexForPress(event); index < kButtonCount)
            m_acceptedButtons[index] = true;
    }

    void acceptProtectedRelease(const hyremote::InputEvent &event) noexcept
    {
        ++m_protectedReleasePending;

        if (isKnownKeyRelease(event)) {
            m_acceptedKeys[keyIndex(event.key)] = false;
            return;
        }

        if (const auto index = buttonIndexForRelease(event); index < kButtonCount)
            m_acceptedButtons[index] = false;
    }

    void removePendingNormal() noexcept
    {
        if (m_normalPending != 0U)
            --m_normalPending;
    }

    // The pending deque has been moved to the GUI-thread batch. Accepted logical hold state spans
    // batches because a later protocol release must still be recognized after its press was drained.
    void pendingBatchTaken() noexcept
    {
        m_normalPending = 0U;
        m_protectedReleasePending = 0U;
    }

    // Terminal adapter shutdown discards every pending event and balances GUI-delivered state via the
    // adapter's existing shutdown path. No accepted protocol hold may survive into a future runtime.
    void resetAll() noexcept
    {
        pendingBatchTaken();
        m_acceptedKeys.fill(false);
        m_acceptedButtons.fill(false);
    }

    std::size_t normalPending() const noexcept { return m_normalPending; }
    std::size_t protectedReleasePending() const noexcept { return m_protectedReleasePending; }

private:
    static constexpr std::size_t kNoButton = kButtonCount;

    static constexpr bool isKnownKeyPress(const hyremote::InputEvent &event) noexcept
    {
        return event.kind == hyremote::InputEventKind::Key && event.pressed
               && event.key != hyremote::KeyCode::Unknown
               && static_cast<std::size_t>(event.key) < kKeySlotCount;
    }

    static constexpr bool isKnownKeyRelease(const hyremote::InputEvent &event) noexcept
    {
        return event.kind == hyremote::InputEventKind::Key && !event.pressed
               && event.key != hyremote::KeyCode::Unknown
               && static_cast<std::size_t>(event.key) < kKeySlotCount;
    }

    static constexpr std::size_t keyIndex(hyremote::KeyCode key) noexcept
    {
        return static_cast<std::size_t>(key);
    }

    static constexpr std::size_t buttonIndex(hyremote::PointerButton button) noexcept
    {
        switch (button) {
        case hyremote::PointerButton::Left:
            return 0;
        case hyremote::PointerButton::Middle:
            return 1;
        case hyremote::PointerButton::Right:
            return 2;
        case hyremote::PointerButton::None:
            return kNoButton;
        }
        return kNoButton;
    }

    static constexpr std::size_t buttonIndexForPress(const hyremote::InputEvent &event) noexcept
    {
        if (event.kind != hyremote::InputEventKind::PointerButton || !event.pressed)
            return kNoButton;
        return buttonIndex(event.button);
    }

    static constexpr std::size_t buttonIndexForRelease(const hyremote::InputEvent &event) noexcept
    {
        if (event.kind != hyremote::InputEventKind::PointerButton || event.pressed)
            return kNoButton;
        return buttonIndex(event.button);
    }

    std::size_t m_normalPending = 0U;
    std::size_t m_protectedReleasePending = 0U;
    std::array<bool, kKeySlotCount> m_acceptedKeys{};
    std::array<bool, kButtonCount> m_acceptedButtons{};
};

static_assert(InputMailboxAdmission::kKnownKeyCount == 69U,
              "V1 protected-release bound must be revisited when KeyCode grows");
static_assert(InputMailboxAdmission::kMaxAcceptedHeldStates == 72U,
              "V1 protected-release held-state bound changed unexpectedly");
static_assert(InputMailboxAdmission::kProtectedReleaseCapacity == 136U,
              "V1 protected-release reserve must remain explicitly bounded");

}  // namespace HyRemote::detail
