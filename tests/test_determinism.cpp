#include <gtest/gtest.h>
#include "match.hpp"
#include "world.hpp"
#include "snapshot.hpp"
#include "config.hpp"
#include "../fixtures/idle_bot.hpp"

// Build a small symmetric world: two Command Cores + two Drones each side.
static game::World make_test_world(uint64_t seed) {
  game::World w;
  w.map = game::Map::make(20, 20);
  w.rng = game::Rng{seed};
  w.rng_seed = seed;

  // Faction 0: top-left
  w.spawn_structure({0}, game::StructureType::CommandCore, {2, 2});
  w.spawn_unit({0}, game::UnitType::Drone, {3, 2});
  w.spawn_unit({0}, game::UnitType::Interceptor, {4, 2});

  // Faction 1: bottom-right
  w.spawn_structure({1}, game::StructureType::CommandCore, {17, 17});
  w.spawn_unit({1}, game::UnitType::Drone, {16, 17});
  w.spawn_unit({1}, game::UnitType::Interceptor, {15, 17});

  return w;
}

// ---- Determinism keystone ------------------------------------------------

TEST(Determinism, SameSeedSameHash) {
  auto run = [](uint64_t seed) {
    game::World w = make_test_world(seed);
    game::IdleBot a, b;
    return game::run_match(w, a, 0, b, 1, 200).hash.fingerprint();
  };

  uint64_t h1 = run(42);
  uint64_t h2 = run(42);
  EXPECT_EQ(h1, h2);
}

TEST(Determinism, DifferentSeedsDifferentHash) {
  auto run = [](uint64_t seed) {
    game::World w = make_test_world(seed);
    game::IdleBot a, b;
    return game::run_match(w, a, 0, b, 1, 200).hash.fingerprint();
  };

  EXPECT_NE(run(1), run(2));
}

TEST(Determinism, PerTickHashesMatch) {
  auto run = [](uint64_t seed) {
    game::World w = make_test_world(seed);
    game::IdleBot a, b;
    return game::run_match(w, a, 0, b, 1, 200).hash.per_tick;
  };

  auto h1 = run(99);
  auto h2 = run(99);
  ASSERT_EQ(h1.size(), h2.size());
  for (size_t i = 0; i < h1.size(); ++i)
    EXPECT_EQ(h1[i], h2[i]) << "diverged at tick " << i;
}

// ---- Replay keystone -----------------------------------------------------

TEST(Determinism, ReplayReproducesHash) {
  game::World w = make_test_world(7);
  game::IdleBot a, b;
  auto rec = game::run_match(w, a, 0, b, 1, 200);

  uint64_t replayed = game::replay(rec.replay);
  EXPECT_EQ(replayed, rec.replay.expected_hash);
}

// ---- Fog-of-war correctness ----------------------------------------------

TEST(Fog, SnapshotNeverLeaksEnemyOutsideVision) {
  game::World w = make_test_world(0);
  // Place a stealth enemy unit far away from faction 0's sight
  w.spawn_unit({1}, game::UnitType::Drone, {19, 19});

  game::Snapshot snap = game::build_snapshot(w, 0);

  // Verify every visible enemy is actually within sight of some friendly unit
  for (const auto& ev : snap.visible_enemies) {
    game::Vec2 epos{ev.x, ev.y};
    bool in_sight = false;
    for (const auto& uv : snap.my_units) {
      game::Vec2 upos{uv.x, uv.y};
      int32_t sight = game::stats_for(static_cast<game::UnitType>(uv.type)).sight;
      if (game::manhattan(upos, epos) <= sight) {
        in_sight = true;
        break;
      }
    }
    EXPECT_TRUE(in_sight) << "enemy at (" << ev.x << "," << ev.y << ") leaked into snapshot";
  }
}

TEST(Fog, FarEnemyNotVisible) {
  game::World w;
  w.map = game::Map::make(40, 40);
  w.rng = game::Rng{0};

  // Faction 0 unit in one corner, faction 1 unit far away
  w.spawn_unit({0}, game::UnitType::Interceptor, {0, 0});
  w.spawn_unit({1}, game::UnitType::Interceptor, {39, 39});

  game::Snapshot snap = game::build_snapshot(w, 0);
  EXPECT_EQ(snap.visible_enemies.size(), 0u);
}
