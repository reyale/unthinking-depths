#include <gtest/gtest.h>
#include "config.hpp"
#include "entity.hpp"
#include "territory.hpp"
#include "world.hpp"

static game::World make_world(int32_t w = 20, int32_t h = 20) {
  game::World world;
  world.map = game::Map::make(w, h);
  world.rng  = game::Rng{0};
  world.tick = 0;
  return world;
}

// ---- Voronoi partition -------------------------------------------------------

TEST(Territory, NoClaimNodesZeroPct) {
  auto w = make_world();
  auto ts = game::run_territory(w, 1000);
  EXPECT_EQ(ts.pct_faction[0], 0u);
  EXPECT_EQ(ts.pct_faction[1], 0u);
}

TEST(Territory, SingleNodeOwnsNearbyTiles) {
  auto w = make_world(20, 20);
  w.spawn_structure({0}, game::StructureType::ClaimNode, {10, 10});
  auto ts = game::run_territory(w, 1000);
  EXPECT_GT(ts.pct_faction[0], 0u);
  EXPECT_EQ(ts.pct_faction[1], 0u);
}

TEST(Territory, TwoOpposingNodesSplitMap) {
  // Symmetric 20×20 map; nodes at opposite corners → roughly equal split.
  auto w = make_world(20, 20);
  w.spawn_structure({0}, game::StructureType::ClaimNode, {0, 10});
  w.spawn_structure({1}, game::StructureType::ClaimNode, {19, 10});
  auto ts = game::run_territory(w, 1000);
  // Each side should hold ~50% (within influence radius).
  EXPECT_GT(ts.pct_faction[0], 0u);
  EXPECT_GT(ts.pct_faction[1], 0u);
  // Neither should be larger than the other by more than a few percent
  // (exact split depends on odd column count and influence radius).
  int32_t diff = static_cast<int32_t>(ts.pct_faction[0]) -
                 static_cast<int32_t>(ts.pct_faction[1]);
  if (diff < 0) diff = -diff;
  EXPECT_LE(diff, 10);
}

TEST(Territory, EquidistantTileIsNeutral) {
  // Two nodes equidistant from tile (10,10) on a 21×21 map so the midpoint
  // is exactly a tile centre.
  auto w = make_world(21, 21);
  w.spawn_structure({0}, game::StructureType::ClaimNode, {5, 10});
  w.spawn_structure({1}, game::StructureType::ClaimNode, {15, 10});
  // Tile (10,10) is exactly equidistant → neutral; total pct must be < 100.
  auto ts = game::run_territory(w, 1000);
  EXPECT_LT(ts.pct_faction[0] + ts.pct_faction[1], 100u);
}

TEST(Territory, TilesBeyondInfluenceAreNeutral) {
  // Place a single node and verify total claimed tiles is bounded by the
  // influence circle area (π*r² ≤ tiles ≤ (2r+1)²).
  auto w = make_world(128, 128);
  w.spawn_structure({0}, game::StructureType::ClaimNode, {64, 64});
  auto ts = game::run_territory(w, 1000);
  // At most (2*CLAIM_INFLUENCE+1)² tiles can be claimed on a 128×128 map.
  uint32_t box_tiles = static_cast<uint32_t>(
    (2 * game::cfg::CLAIM_INFLUENCE + 1) * (2 * game::cfg::CLAIM_INFLUENCE + 1));
  uint32_t total_tiles = 128u * 128u;
  uint32_t max_pct = (box_tiles * 100) / total_tiles + 1; // +1 for rounding
  EXPECT_LE(ts.pct_faction[0], max_pct);
}

TEST(Territory, DeadNodeNotCounted) {
  auto w = make_world(20, 20);
  auto& s = w.spawn_structure({0}, game::StructureType::ClaimNode, {10, 10});
  s.hp = 0; // kill it
  auto ts = game::run_territory(w, 1000);
  EXPECT_EQ(ts.pct_faction[0], 0u);
}

// ---- Claim Node fragility ---------------------------------------------------

TEST(Territory, FragilityClampsFutureHp) {
  auto w = make_world();
  auto& s = w.spawn_structure({0}, game::StructureType::ClaimNode, {5, 5});
  s.hp = game::cfg::CLAIM_HP_BASE; // full health

  // Run at half-time → eff_hp ≈ BASE/2 + FLOOR; hp should be clamped.
  uint32_t tick_cap = 1000;
  w.tick = tick_cap / 2;
  game::run_territory(w, tick_cap);

  int32_t expected_eff =
    game::cfg::CLAIM_HP_BASE / 2 + game::cfg::CLAIM_HP_FLOOR;
  EXPECT_LE(s.hp, expected_eff);
  EXPECT_GT(s.hp, 0);
}

TEST(Territory, FragilityFloorAtTickCap) {
  auto w = make_world();
  auto& s = w.spawn_structure({0}, game::StructureType::ClaimNode, {5, 5});
  s.hp = game::cfg::CLAIM_HP_BASE;

  uint32_t tick_cap = 100;
  w.tick = tick_cap; // at or past cap
  game::run_territory(w, tick_cap);

  EXPECT_EQ(s.hp, game::cfg::CLAIM_HP_FLOOR);
}

TEST(Territory, FragilityDoesNotIncreaseHp) {
  auto w = make_world();
  auto& s = w.spawn_structure({0}, game::StructureType::ClaimNode, {5, 5});
  s.hp = 1; // already low
  w.tick = 0;
  game::run_territory(w, 1000);
  EXPECT_EQ(s.hp, 1); // should not be raised
}

// ---- Win threshold ----------------------------------------------------------

TEST(Territory, ThresholdStartsHigh) {
  // At tick 0 threshold = THRESH_A.
  EXPECT_EQ(game::win_threshold(0, 1000), game::cfg::THRESH_A);
}

TEST(Territory, ThresholdDropsOverTime) {
  int32_t early = game::win_threshold(100, 1000);
  int32_t late  = game::win_threshold(900, 1000);
  EXPECT_GT(early, late);
}

TEST(Territory, ThresholdFloorsAtMin) {
  // At tick_cap the threshold must equal THRESH_MIN.
  int32_t t = game::win_threshold(1000, 1000);
  EXPECT_EQ(t, game::cfg::THRESH_MIN);
}
