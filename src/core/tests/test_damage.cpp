// SPDX-License-Identifier: Apache-2.0
// C2 - damage semantics: Unknown | FullFrame | Regions, with empty Regions explicitly not
// Unknown, and frame/root coordinates preserved by Core.

#include "fakes.hpp"

using namespace hyremote;
using namespace hyremote::test;

HYR_TEST(the_three_damage_states_are_distinguishable)
{
    const Damage unknown;
    HYR_CHECK(unknown.kind == DamageKind::Unknown);
    HYR_CHECK(!unknown.hasRegions());

    const Damage full = Damage::fullFrame();
    HYR_CHECK(full.kind == DamageKind::FullFrame);
    HYR_CHECK(!full.hasRegions());

    const Damage regions = Damage::fromRegions({{0, 0, 4, 4}});
    HYR_CHECK(regions.kind == DamageKind::Regions);
    HYR_CHECK(regions.hasRegions());

    HYR_CHECK(unknown.kind != full.kind);
    HYR_CHECK(full.kind != regions.kind);
    HYR_CHECK(unknown.kind != regions.kind);
}

HYR_TEST(empty_regions_is_not_unknown)
{
    const Damage empty = Damage::fromRegions({});

    // "region tracking is available and nothing changed" is a third state, not "unknown".
    HYR_CHECK(empty.kind == DamageKind::Regions);
    HYR_CHECK(!empty.hasRegions());
    HYR_CHECK(empty.kind != DamageKind::Unknown);
    HYR_CHECK(empty.kind != DamageKind::FullFrame);
}

HYR_TEST(regions_preserve_frame_root_coordinates)
{
    const std::vector<Rect> rects{{-3, 7, 10, 4}, {0, 0, 1, 1}, {100, 200, 300, 400}};

    RemoteFrame frame = makeFrame(640, 480);
    frame.damage = Damage::fromRegions(rects);

    HYR_CHECK(validateFrame(frame).ok);
    HYR_CHECK_EQ(frame.damage.regions.size(), std::size_t{3});
    HYR_CHECK_EQ(frame.damage.regions[0].x, -3);
    HYR_CHECK_EQ(frame.damage.regions[0].y, 7);
    HYR_CHECK_EQ(frame.damage.regions[0].width, std::uint32_t{10});
    HYR_CHECK_EQ(frame.damage.regions[2].x, 100);
    HYR_CHECK_EQ(frame.damage.regions[2].height, std::uint32_t{400});
}

HYR_TEST(core_passes_damage_through_unchanged)
{
    RunningSession running;
    HYR_CHECK(running.start());
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);

    RemoteFrame unknownDamage = makeFrame(64, 32);
    unknownDamage.damage = Damage::unknown();
    HYR_CHECK(running.source->deliver(unknownDamage));

    HYR_CHECK(running.transport->waitForAccepted(1));
    const std::vector<RemoteFrame> frames = running.transport->frames();
    HYR_CHECK_EQ(frames.size(), std::size_t{1});
    HYR_CHECK(frames[0].damage.kind == DamageKind::Unknown);
    HYR_CHECK(frames[0].damage.regions.empty());

    // FullFrame and Regions survive as well, and Core never clips or invents rectangles.
    RemoteFrame regions = makeFrame(64, 32);
    regions.damage = Damage::fromRegions({{2, 4, 6, 8}});
    HYR_CHECK(running.source->deliver(regions));
    HYR_CHECK(running.transport->waitForAccepted(2));

    const std::vector<RemoteFrame> all = running.transport->frames();
    HYR_CHECK_EQ(all.size(), std::size_t{2});
    HYR_CHECK(all[1].damage.kind == DamageKind::Regions);
    HYR_CHECK_EQ(all[1].damage.regions.size(), std::size_t{1});
    HYR_CHECK_EQ(all[1].damage.regions[0].x, 2);
    HYR_CHECK_EQ(all[1].damage.regions[0].y, 4);
    HYR_CHECK_EQ(all[1].damage.regions[0].width, std::uint32_t{6});
    HYR_CHECK_EQ(all[1].damage.regions[0].height, std::uint32_t{8});

    RemoteFrame full = makeFrame(64, 32);
    full.damage = Damage::fullFrame();
    HYR_CHECK(running.source->deliver(full));
    HYR_CHECK(running.transport->waitForAccepted(3));
    HYR_CHECK(running.transport->frames()[2].damage.kind == DamageKind::FullFrame);
}

HYR_TEST(an_empty_region_list_survives_the_pipeline_as_regions)
{
    RunningSession running;
    HYR_CHECK(running.start());

    RemoteFrame frame = makeFrame(32, 16);
    frame.damage = Damage::fromRegions({});
    HYR_CHECK(running.source->deliver(frame));

    HYR_CHECK(running.transport->waitForAccepted(1));
    const std::vector<RemoteFrame> frames = running.transport->frames();
    HYR_CHECK_EQ(frames.size(), std::size_t{1});
    HYR_CHECK(frames[0].damage.kind == DamageKind::Regions);
    HYR_CHECK(frames[0].damage.regions.empty());
    HYR_CHECK(!frames[0].damage.hasRegions());
}

HYR_TEST_MAIN()
