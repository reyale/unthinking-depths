#include <gtest/gtest.h>
#include "world.hpp"

static game::World make_world() {
  game::World w;
  w.map = game::Map::make(10, 10);
  w.rng = game::Rng{0};
  return w;
}

TEST(World, SpawnUnitAssignsAscendingIds) {
  auto w = make_world();
  auto& a = w.spawn_unit({0}, game::UnitType::Drone, {1, 1});
  auto& b = w.spawn_unit({0}, game::UnitType::Interceptor, {2, 2});
  EXPECT_EQ(a.id.value, 1u);
  EXPECT_EQ(b.id.value, 2u);
  EXPECT_LT(a.id, b.id);
}

TEST(World, SpawnUnitSetsHpFromStats) {
  auto w = make_world();
  auto& u = w.spawn_unit({0}, game::UnitType::Frigate, {0, 0});
  EXPECT_EQ(u.hp, game::cfg::FRIGATE_HP);
}

TEST(World, SpawnStructureSetsHp) {
  auto w = make_world();
  auto& s = w.spawn_structure({0}, game::StructureType::CommandCore, {5, 5});
  EXPECT_EQ(s.hp, game::cfg::CMD_CORE_HP);
}

TEST(World, FindUnitReturnsNullForMissing) {
  auto w = make_world();
  EXPECT_EQ(w.find_unit(game::UnitId{99}), nullptr);
}

TEST(World, PurgeDeadRemovesKilledUnits) {
  auto w = make_world();
  auto& u = w.spawn_unit({0}, game::UnitType::Drone, {1, 1});
  u.hp = 0;
  w.purge_dead();
  EXPECT_EQ(w.units.size(), 0u);
}

TEST(World, CommandCoreFindsLivingCore) {
  auto w = make_world();
  game::FactionId f0{0};
  w.spawn_structure(f0, game::StructureType::CommandCore, {5, 5});
  EXPECT_NE(w.command_core(f0), nullptr);
}

TEST(World, CommandCoreNullAfterDestroyed) {
  auto w = make_world();
  game::FactionId f0{0};
  auto& s = w.spawn_structure(f0, game::StructureType::CommandCore, {5, 5});
  s.hp = 0;
  w.purge_dead();
  EXPECT_EQ(w.command_core(f0), nullptr);
}

TEST(Grid, PassabilityByTerrain) {
  auto m = game::Map::make(4, 4);
  m.set_terrain({1, 1}, game::Terrain::Asteroid);
  EXPECT_FALSE(m.passable({1, 1}));
  EXPECT_TRUE(m.passable({0, 0}));
}

TEST(Grid, OutOfBoundsNotPassable) {
  auto m = game::Map::make(4, 4);
  EXPECT_FALSE(m.passable({-1, 0}));
  EXPECT_FALSE(m.passable({4, 0}));
}

TEST(Grid, ManhattanDistance) {
  EXPECT_EQ(game::manhattan({0, 0}, {3, 4}), 7);
  EXPECT_EQ(game::manhattan({2, 2}, {2, 2}), 0);
}
