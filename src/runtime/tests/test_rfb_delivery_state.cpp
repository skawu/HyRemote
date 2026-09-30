#include <QByteArray>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>

#include "hyremote/core/frame.hpp"
#include "hyremote/core/storage.hpp"
#include "transport/rfb_delivery_state.hpp"
#include "transport/rfb_trle.hpp"

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

hyremote::RemoteFrame makeFrame(std::uint32_t width,
                                std::uint32_t height,
                                std::uint32_t background,
                                std::optional<HyRemote::detail::RfbRect> changed = std::nullopt,
                                std::uint32_t changedColor = 0)
{
    auto storage = hyremote::CpuFrameStorage::createSinglePlane(width * 4U, height);
    CHECK(storage != nullptr);
    if (!storage)
        return {};

    auto *bytes = reinterpret_cast<std::uint8_t *>(storage->mutablePlane(0));
    CHECK(bytes != nullptr);
    if (!bytes)
        return {};

    const auto writePixel = [&](std::uint32_t x, std::uint32_t y, std::uint32_t rgb) {
        auto *pixel = bytes + (static_cast<std::size_t>(y) * width + x) * 4U;
        pixel[0] = static_cast<std::uint8_t>((rgb >> 16U) & 0xffU);
        pixel[1] = static_cast<std::uint8_t>((rgb >> 8U) & 0xffU);
        pixel[2] = static_cast<std::uint8_t>(rgb & 0xffU);
        pixel[3] = 0xffU;
    };

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x)
            writePixel(x, y, background);
    }
    if (changed) {
        const auto x1 = std::min<std::uint32_t>(width, changed->x + changed->width);
        const auto y1 = std::min<std::uint32_t>(height, changed->y + changed->height);
        for (std::uint32_t y = static_cast<std::uint32_t>(changed->y); y < y1; ++y) {
            for (std::uint32_t x = static_cast<std::uint32_t>(changed->x); x < x1; ++x)
                writePixel(x, y, changedColor);
        }
    }

    hyremote::RemoteFrame frame;
    frame.geometry.size = {width, height};
    frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
    frame.geometry.alphaMode = hyremote::AlphaMode::Opaque;
    frame.geometry.planeCount = 1;
    frame.storage = std::move(storage);
    frame.timing.pts = hyremote::Clock::now();
    frame.timing.ptsSource = hyremote::PtsSource::Completion;
    frame.damage = hyremote::Damage::fullFrame();
    return frame;
}

void testChangedTilesAndBounds()
{
    using namespace HyRemote::detail;
    const auto base = makeFrame(192, 128, 0x203040U);
    const auto changed = makeFrame(192, 128, 0x203040U, RfbRect{70, 20, 20, 20}, 0xe0a030U);
    const RfbDamageRegion damage = changedRfbTiles(&base, changed);
    CHECK(!damage.empty());
    CHECK(damage.size() == 1U);
    CHECK(damage.rects().front() == (RfbRect{64, 0, 64, 64}));

    RfbDamageRegion bounded;
    for (int i = 0; i < 100; ++i)
        bounded.add({i * 2, i * 2, 1, 1});
    CHECK(bounded.size() <= kRfbMaxDamageRects);
}

void testPendingRequestAndCommitBoundary()
{
    using namespace HyRemote::detail;
    RfbUpdateState state;
    const RfbRect full{0, 0, 128, 128};
    state.request(true, full);
    CHECK(state.hasOutstandingRequest());
    CHECK(!state.selectEligible().has_value());

    RfbDamageRegion change;
    change.add({0, 0, 64, 64});
    state.accumulate(change);
    const auto selected = state.selectEligible();
    CHECK(selected.has_value());
    CHECK(selected && selected->size() == 1U);

    // Selection is not delivery: a failed encode/write leaves both request and damage pending.
    const auto retry = state.selectEligible();
    CHECK(retry.has_value());
    CHECK(state.hasOutstandingRequest());
    CHECK(state.hasPendingDamage());

    if (retry)
        state.commitDelivered(*retry);
    CHECK(!state.hasOutstandingRequest());
    CHECK(!state.hasPendingDamage());
}

void testPartialAndResize()
{
    using namespace HyRemote::detail;
    RfbUpdateState state;
    RfbDamageRegion damage;
    damage.add({0, 0, 64, 64});
    damage.add({128, 0, 64, 64});
    state.accumulate(damage);

    state.request(true, {0, 0, 64, 64});
    const auto left = state.selectEligible();
    CHECK(left.has_value());
    if (left)
        state.commitDelivered(*left);
    CHECK(state.hasPendingDamage());

    state.request(true, {128, 0, 64, 64});
    const auto right = state.selectEligible();
    CHECK(right.has_value());
    if (right)
        state.commitDelivered(*right);
    CHECK(!state.hasPendingDamage());

    state.invalidateFull(320, 200);
    state.request(true, {10, 10, 10, 10});
    const auto resized = state.selectEligible();
    CHECK(resized.has_value());
    CHECK(resized && resized->size() == 1U);
    CHECK(resized && resized->rects().front() == (RfbRect{0, 0, 320, 200}));
}

void testNativeTrlePayload()
{
    using namespace HyRemote::detail;
    const auto frame = makeFrame(32, 16, 0x112233U, RfbRect{16, 0, 16, 16}, 0xa0b0c0U);
    QByteArray encoded;
    CHECK(appendNativeTrlePayload(encoded, frame, 0, 0, 32, 16));
    // Two 16x16 solid tiles: [subencoding=1, R,G,B] twice.
    CHECK(encoded.size() == 8);
    CHECK(static_cast<unsigned char>(encoded.at(0)) == 1U);
    CHECK(static_cast<unsigned char>(encoded.at(1)) == 0x11U);
    CHECK(static_cast<unsigned char>(encoded.at(2)) == 0x22U);
    CHECK(static_cast<unsigned char>(encoded.at(3)) == 0x33U);
    CHECK(static_cast<unsigned char>(encoded.at(4)) == 1U);
    CHECK(static_cast<unsigned char>(encoded.at(5)) == 0xa0U);
    CHECK(static_cast<unsigned char>(encoded.at(6)) == 0xb0U);
    CHECK(static_cast<unsigned char>(encoded.at(7)) == 0xc0U);
}

}  // namespace

int main()
{
    testChangedTilesAndBounds();
    testPendingRequestAndCommitBoundary();
    testPartialAndResize();
    testNativeTrlePayload();
    return failures == 0 ? 0 : 1;
}
