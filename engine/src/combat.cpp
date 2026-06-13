#include "combat.hpp"
#include "world.hpp"
#include "entity.hpp"
#include "grid.hpp"
#include <map>
#include <limits>

namespace game {

// Finds the best target for `attacker`: explicit target if valid and in range,
// else nearest enemy in range (Manhattan), else UnitId{} (null).
static UnitId pick_target(const World& world, const Unit& attacker, const Command* cmd) {
  const int32_t range = attacker.stats().range;
  if (range == 0)
    return UnitId{};

  // Explicit target from command
  if (cmd && cmd->target_id != 0) {
    UnitId tid{cmd->target_id};
    const Unit* tgt = world.find_unit(tid);
    if (tgt && tgt->alive() && tgt->faction != attacker.faction &&
        manhattan(attacker.pos, tgt->pos) <= range)
      return tid;
  }

  // Nearest enemy in range; ties broken by ascending unit-id
  UnitId best{};
  int32_t best_dist = std::numeric_limits<int32_t>::max();
  for (const auto& [eid, enemy] : world.units) {
    if (enemy.faction == attacker.faction || !enemy.alive())
      continue;
    int32_t d = manhattan(attacker.pos, enemy.pos);
    if (d > range)
      continue;
    if (d < best_dist || (d == best_dist && eid < best)) {
      best_dist = d;
      best = eid;
    }
  }
  return best;
}

// Applies splash damage centered on `center`, radius 1, to all living units.
static void apply_splash(World& world, Vec2 center, int32_t dmg,
                         std::map<UnitId, int32_t>& pending_damage) {
  for (auto& [uid, unit] : world.units) {
    if (!unit.alive())
      continue;
    if (manhattan(unit.pos, center) <= 1)
      pending_damage[uid] += dmg;
  }
}

void run_combat(World& world, const ValidatedCommands& cmds_a, const ValidatedCommands& cmds_b) {
  // Merge commands for target lookup (Attack/MoveAttack provide targets).
  ValidatedCommands all_cmds;
  for (const auto& [k, v] : cmds_a)
    all_cmds.emplace(k, v);
  for (const auto& [k, v] : cmds_b)
    all_cmds.emplace(k, v);

  auto get_cmd = [&](UnitId uid) -> const Command* {
    auto it = all_cmds.find(uid);
    return it != all_cmds.end() ? &it->second : nullptr;
  };

  // --- Sub-phase A: first-strike (Frigates only) -----------------------
  // Iterate in ascending unit-id order; draw RNG in that order.
  for (auto& [uid, unit] : world.units) {
    if (!unit.alive() || !unit.stats().first_strike)
      continue;
    const Command* cmd = get_cmd(uid);
    // Only attack if ordered to Attack or MoveAttack (or no-move Attack)
    auto kind = cmd ? static_cast<CommandKind>(cmd->kind) : CommandKind::Move;
    if (kind != CommandKind::Attack && kind != CommandKind::MoveAttack)
      continue;

    UnitId tid = pick_target(world, unit, cmd);
    if (tid.is_null())
      continue;
    Unit* tgt = world.find_unit(tid);
    if (!tgt)
      continue;

    // Draw RNG (even if hit is deterministic — keeps draw count consistent)
    world.rng.next();
    tgt->hp -= unit.stats().dmg;
    if (!tgt->alive())
      tgt->hp = 0;
  }

  // Apply first-strike deaths now (before simultaneous phase)
  world.purge_dead();

  // --- Sub-phase B: simultaneous ----------------------------------------
  // Compute all damage against survivors, collect into pending map, then apply.
  std::map<UnitId, int32_t> pending;

  for (auto& [uid, unit] : world.units) {
    if (!unit.alive() || unit.stats().first_strike)
      continue;
    const Command* cmd = get_cmd(uid);
    auto kind = cmd ? static_cast<CommandKind>(cmd->kind) : CommandKind::Move;
    if (kind != CommandKind::Attack && kind != CommandKind::MoveAttack)
      continue;

    UnitId tid = pick_target(world, unit, cmd);
    if (tid.is_null())
      continue;
    Unit* tgt = world.find_unit(tid);
    if (!tgt)
      continue;

    world.rng.next(); // maintain consistent draw count

    if (unit.stats().splash) {
      apply_splash(world, tgt->pos, unit.stats().dmg, pending);
    } else {
      pending[tid] += unit.stats().dmg;
    }
  }

  // Apply simultaneous damage
  for (auto& [uid, dmg] : pending) {
    Unit* u = world.find_unit(uid);
    if (u)
      u->hp -= dmg;
  }

  world.purge_dead();
}

} // namespace game
