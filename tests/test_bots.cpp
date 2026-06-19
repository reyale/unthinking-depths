#include <gtest/gtest.h>
#include "match.hpp"
#include "world.hpp"
#include "grid.hpp"
#include "../../fixtures/rush_bot.hpp"
#include "../../fixtures/econ_bot.hpp"
#include "../../fixtures/idle_bot.hpp"

// 20×20 map with resource nodes visible from each faction's starting drone
// (Manhattan distance = 2 from the drone's start, within sight radius 4).
// Point-symmetric: faction 0 top-left, faction 1 bottom-right.
static game::World make_bot_world(uint64_t seed = 42) {
  game::World w;
  w.map = game::Map::make(20, 20);
  w.rng = game::Rng{seed};
  w.rng_seed = seed;

  w.map.set_terrain({5,  2},  game::Terrain::ResourceNode, 1000);
  w.map.set_terrain({14, 17}, game::Terrain::ResourceNode, 1000);
  w.map.recount_passable();

  // Faction 0 — top-left
  w.spawn_structure({0}, game::StructureType::CommandCore, {2, 2});
  w.spawn_unit({0}, game::UnitType::Drone,       {3, 2});
  w.spawn_unit({0}, game::UnitType::Interceptor, {4, 2});
  w.spawn_unit({0}, game::UnitType::Frigate,     {4, 3});

  // Faction 1 — bottom-right
  w.spawn_structure({1}, game::StructureType::CommandCore, {17, 17});
  w.spawn_unit({1}, game::UnitType::Drone,       {16, 17});
  w.spawn_unit({1}, game::UnitType::Interceptor, {15, 17});
  w.spawn_unit({1}, game::UnitType::Frigate,     {15, 16});

  return w;
}

// ---- RushBot ---------------------------------------------------------------

TEST(RushBot, RunsFullMatch) {
  auto w = make_bot_world();
  game::RushBot a, b;
  auto rec = game::run_match(w, a, 0, b, 1, 200);
  EXPECT_TRUE(a.healthy());
  EXPECT_TRUE(b.healthy());
}

TEST(RushBot, Deterministic) {
  auto run = [](uint64_t seed) {
    auto w = make_bot_world(seed);
    game::RushBot a, b;
    return game::run_match(w, a, 0, b, 1, 200).replay.expected_hash;
  };
  for (uint64_t seed : {1ULL, 42ULL, 99ULL})
    EXPECT_EQ(run(seed), run(seed)) << "non-deterministic at seed " << seed;
}

TEST(RushBot, ReplayReproducesHash) {
  auto w = make_bot_world(7);
  game::RushBot a, b;
  auto rec = game::run_match(w, a, 0, b, 1, 200);
  EXPECT_EQ(game::replay(rec.replay), rec.replay.expected_hash);
}

// Both sides rush with MoveAttack — their Frigates will clash in the middle.
// Verify the replay log has non-empty command buffers (bots actually issued orders).
TEST(RushBot, VsAggroIssuesCommands) {
  auto w = make_bot_world(13);
  game::RushBot a, b;
  auto rec = game::run_match(w, a, 0, b, 1, 200);

  bool any_a = false, any_b = false;
  for (const auto& entry : rec.replay.ticks) {
    if (!entry.raw_a.empty()) any_a = true;
    if (!entry.raw_b.empty()) any_b = true;
  }
  EXPECT_TRUE(any_a);
  EXPECT_TRUE(any_b);
}

// ---- EconBot ----------------------------------------------------------------

TEST(EconBot, RunsFullMatch) {
  auto w = make_bot_world();
  game::EconBot a, b;
  auto rec = game::run_match(w, a, 0, b, 1, 200);
  EXPECT_TRUE(a.healthy());
  EXPECT_TRUE(b.healthy());
  // EconBot only moves drones; combat units hold position and never advance on
  // the enemy CommandCore.  Match ends at tick cap.
  EXPECT_EQ(rec.outcome.reason, game::WinReason::Draw);
}

TEST(EconBot, Deterministic) {
  auto run = [](uint64_t seed) {
    auto w = make_bot_world(seed);
    game::EconBot a, b;
    return game::run_match(w, a, 0, b, 1, 200).replay.expected_hash;
  };
  for (uint64_t seed : {2ULL, 55ULL, 88ULL})
    EXPECT_EQ(run(seed), run(seed)) << "non-deterministic at seed " << seed;
}

TEST(EconBot, ReplayReproducesHash) {
  auto w = make_bot_world(3);
  game::EconBot a, b;
  auto rec = game::run_match(w, a, 0, b, 1, 200);
  EXPECT_EQ(game::replay(rec.replay), rec.replay.expected_hash);
}

// ---- RushBot vs EconBot ----------------------------------------------------

TEST(RushVsEcon, Deterministic) {
  auto run = [](uint64_t seed) {
    auto w = make_bot_world(seed);
    game::RushBot a;
    game::EconBot b;
    return game::run_match(w, a, 0, b, 1, 200).replay.expected_hash;
  };
  for (uint64_t seed : {10ULL, 20ULL, 30ULL})
    EXPECT_EQ(run(seed), run(seed)) << "non-deterministic at seed " << seed;
}

TEST(RushVsEcon, ReplayReproducesHash) {
  auto w = make_bot_world(50);
  game::RushBot a;
  game::EconBot b;
  auto rec = game::run_match(w, a, 0, b, 1, 200);
  EXPECT_EQ(game::replay(rec.replay), rec.replay.expected_hash);
}

// RushBot issues Move/MoveAttack commands every tick; IdleBot never does.
// The replay log should reflect this asymmetry.
TEST(RushVsEcon, AggroIssuedCommandsIdleDidNot) {
  auto w = make_bot_world(77);
  game::RushBot a;
  game::IdleBot b;
  auto rec = game::run_match(w, a, 0, b, 1, 50);

  bool a_issued = false, b_issued = false;
  for (const auto& entry : rec.replay.ticks) {
    if (!entry.raw_a.empty()) a_issued = true;
    if (!entry.raw_b.empty()) b_issued = true;
  }
  EXPECT_TRUE(a_issued);
  EXPECT_FALSE(b_issued);
}
