#include <gtest/gtest.h>
#include "combat.hpp"
#include "command.hpp"
#include "world.hpp"

// Build a minimal world with two units adjacent to each other.
static game::World make_combat_world() {
  game::World w;
  w.map = game::Map::make(10, 10);
  w.rng = game::Rng{0};
  return w;
}

// Issue an Attack command for a unit.
static game::ValidatedCommands attack_cmd(game::UnitId attacker) {
  game::Command cmd{};
  cmd.unit_id = attacker.value;
  cmd.kind = static_cast<uint16_t>(game::CommandKind::Attack);
  game::ValidatedCommands vc;
  vc.emplace(attacker, cmd);
  return vc;
}

// ---- First-strike --------------------------------------------------------

TEST(Combat, FrigateFirstStrikeKillsBeforeSimultaneous) {
  auto w = make_combat_world();
  // Frigate (faction 0) vs Interceptor (faction 1) at range 1
  auto& frigate = w.spawn_unit({0}, game::UnitType::Frigate, {5, 5});
  auto& inter = w.spawn_unit({1}, game::UnitType::Interceptor, {6, 5});

  // Both attack each other
  auto cmds_a = attack_cmd(frigate.id);
  auto cmds_b = attack_cmd(inter.id);

  game::run_combat(w, cmds_a, cmds_b);

  // Frigate fires first-strike: Interceptor (hp=60, dmg=14) should take 14 dmg
  // but the Frigate (hp=35) fires first and the Interceptor is dead or weakened
  // before firing. Interceptor dmg=12, so:
  //   Frigate first-strike: Interceptor hp 60 → 46 (not dead)
  //   Interceptor simultaneous: Frigate hp 35 → 23
  EXPECT_EQ(inter.hp, game::cfg::INTERCEPTOR_HP - game::cfg::FRIGATE_DMG);
  EXPECT_EQ(frigate.hp, game::cfg::FRIGATE_HP - game::cfg::INTERCEPTOR_DMG);
}

TEST(Combat, FrigateKillsWeakTargetBeforeSimultaneous) {
  auto w = make_combat_world();
  // Frigate vs a heavily damaged Interceptor (1 hp) — first-strike kills it,
  // so it should NOT fire back in the simultaneous phase.
  game::UnitId frigate_id = w.spawn_unit({0}, game::UnitType::Frigate, {5, 5}).id;
  game::UnitId inter_id = w.spawn_unit({1}, game::UnitType::Interceptor, {6, 5}).id;
  w.find_unit(inter_id)->hp = 1;

  auto cmds_a = attack_cmd(frigate_id);
  auto cmds_b = attack_cmd(inter_id);

  game::run_combat(w, cmds_a, cmds_b);

  // Interceptor dead from first-strike; Frigate takes no damage
  EXPECT_EQ(w.find_unit(frigate_id)->hp, game::cfg::FRIGATE_HP);
  EXPECT_EQ(w.find_unit(inter_id), nullptr); // purged
}

// ---- Simultaneous kills --------------------------------------------------

TEST(Combat, MutualKillBothDie) {
  auto w = make_combat_world();
  // Two Interceptors at 1 hp each — simultaneous, both should die
  auto& a = w.spawn_unit({0}, game::UnitType::Interceptor, {5, 5});
  auto& b = w.spawn_unit({1}, game::UnitType::Interceptor, {6, 5});
  a.hp = 1;
  b.hp = 1;

  auto cmds_a = attack_cmd(a.id);
  auto cmds_b = attack_cmd(b.id);

  game::run_combat(w, cmds_a, cmds_b);

  EXPECT_TRUE(w.units.empty());
}

// ---- Artillery splash ----------------------------------------------------

TEST(Combat, ArtillerySplashHitsFriendly) {
  auto w = make_combat_world();
  // Artillery (faction 0) fires at an enemy; a friendly unit is also in splash radius
  auto& arty = w.spawn_unit({0}, game::UnitType::Artillery, {5, 5});
  w.spawn_unit({1}, game::UnitType::Interceptor, {7, 5}); // range 3, in range
  auto& friendly =
    w.spawn_unit({0}, game::UnitType::Drone, {7, 6}); // adjacent to enemy (splash r=1)

  auto cmds_a = attack_cmd(arty.id);
  game::ValidatedCommands empty_b;

  int32_t friendly_hp_before = friendly.hp;
  game::run_combat(w, cmds_a, empty_b);

  // Friendly drone should have taken splash damage
  EXPECT_LT(friendly.hp, friendly_hp_before);
}
