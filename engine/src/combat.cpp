#include "combat.hpp"
#include "world.hpp"
#include "entity.hpp"
#include "grid.hpp"
#include <map>
#include <limits>

namespace game {

// A combat target is either an enemy unit or an enemy structure.
struct CombatTarget {
  UnitId      unit_id{};
  StructureId struct_id{};
  bool is_null() const { return unit_id.is_null() && struct_id.is_null(); }
};

// Pick the best target for `attacker`.  Priority order:
//   1. Explicit unit target from command (if valid and in range)
//   2. Explicit structure target from command (if valid and in range)
//   3. Nearest enemy unit in range (ties: ascending unit-id)
//   4. Nearest enemy structure in range (ties: ascending structure-id)
static CombatTarget pick_target(const World& world, const Unit& attacker, const Command* cmd) {
  const int32_t range = attacker.stats().range;
  if (range == 0)
    return {};

  if (cmd && cmd->target_id != 0) {
    // Try as a unit first
    UnitId uid{cmd->target_id};
    const Unit* u = world.find_unit(uid);
    if (u && u->alive() && u->faction != attacker.faction &&
        manhattan(attacker.pos, u->pos) <= range)
      return {uid, {}};

    // Try as a structure
    StructureId sid{cmd->target_id};
    const Structure* s = world.find_structure(sid);
    if (s && s->alive() && s->faction != attacker.faction &&
        manhattan(attacker.pos, s->pos) <= range)
      return {{}, sid};
  }

  // Auto: nearest enemy unit, ties broken by ascending unit-id
  UnitId best_unit{};
  int32_t best_dist = std::numeric_limits<int32_t>::max();
  for (const auto& [eid, enemy] : world.units) {
    if (enemy.faction == attacker.faction || !enemy.alive())
      continue;
    int32_t d = manhattan(attacker.pos, enemy.pos);
    if (d > range)
      continue;
    if (d < best_dist || (d == best_dist && eid < best_unit)) {
      best_dist = d;
      best_unit = eid;
    }
  }
  if (!best_unit.is_null())
    return {best_unit, {}};

  // Auto fallback: nearest enemy structure, ties broken by ascending structure-id
  StructureId best_struct{};
  int32_t best_struct_dist = std::numeric_limits<int32_t>::max();
  for (const auto& [sid, s] : world.structures) {
    if (s.faction == attacker.faction || !s.alive())
      continue;
    int32_t d = manhattan(attacker.pos, s.pos);
    if (d > range)
      continue;
    if (d < best_struct_dist || (d == best_struct_dist && sid < best_struct)) {
      best_struct_dist = d;
      best_struct = sid;
    }
  }
  return {{}, best_struct};
}

// Splash damage centered on `center`, radius 1.  Hits all living units and
// structures (friendly fire applies to both).
static void apply_splash(World& world, Vec2 center, int32_t dmg,
                         std::map<UnitId, int32_t>&      pending_units,
                         std::map<StructureId, int32_t>& pending_structs) {
  for (const auto& [uid, unit] : world.units) {
    if (!unit.alive())
      continue;
    if (manhattan(unit.pos, center) <= 1)
      pending_units[uid] += dmg;
  }
  for (const auto& [sid, s] : world.structures) {
    if (!s.alive())
      continue;
    if (manhattan(s.pos, center) <= 1)
      pending_structs[sid] += dmg;
  }
}

void run_combat(World& world, const ValidatedCommands& cmds_a, const ValidatedCommands& cmds_b) {
  ValidatedCommands all_cmds;
  for (const auto& [k, v] : cmds_a)
    all_cmds.emplace(k, v);
  for (const auto& [k, v] : cmds_b)
    all_cmds.emplace(k, v);

  auto get_cmd = [&](UnitId uid) -> const Command* {
    auto it = all_cmds.find(uid);
    return it != all_cmds.end() ? &it->second : nullptr;
  };

  // --- Sub-phase A: first-strike (Frigates only) ---------------------------
  for (auto& [uid, unit] : world.units) {
    if (!unit.alive() || !unit.stats().first_strike)
      continue;
    const Command* cmd = get_cmd(uid);
    auto kind = cmd ? static_cast<CommandKind>(cmd->kind) : CommandKind::Move;
    if (kind != CommandKind::Attack && kind != CommandKind::MoveAttack)
      continue;

    CombatTarget tgt = pick_target(world, unit, cmd);
    if (tgt.is_null())
      continue;

    world.rng.next(); // maintain consistent draw count
    if (!tgt.struct_id.is_null()) {
      Structure* s = world.find_structure(tgt.struct_id);
      if (s)
        s->hp -= unit.stats().dmg;
    } else {
      Unit* t = world.find_unit(tgt.unit_id);
      if (t) {
        t->hp -= unit.stats().dmg;
        if (!t->alive())
          t->hp = 0;
      }
    }
  }
  world.purge_dead();

  // --- Sub-phase B: simultaneous -------------------------------------------
  std::map<UnitId, int32_t>      pending_units;
  std::map<StructureId, int32_t> pending_structs;

  for (auto& [uid, unit] : world.units) {
    if (!unit.alive() || unit.stats().first_strike)
      continue;
    const Command* cmd = get_cmd(uid);
    auto kind = cmd ? static_cast<CommandKind>(cmd->kind) : CommandKind::Move;
    if (kind != CommandKind::Attack && kind != CommandKind::MoveAttack)
      continue;

    CombatTarget tgt = pick_target(world, unit, cmd);
    if (tgt.is_null())
      continue;

    world.rng.next();

    if (!tgt.struct_id.is_null()) {
      Structure* s = world.find_structure(tgt.struct_id);
      if (!s)
        continue;
      if (unit.stats().splash)
        apply_splash(world, s->pos, unit.stats().dmg, pending_units, pending_structs);
      else
        pending_structs[tgt.struct_id] += unit.stats().dmg;
    } else {
      Unit* t = world.find_unit(tgt.unit_id);
      if (!t)
        continue;
      if (unit.stats().splash)
        apply_splash(world, t->pos, unit.stats().dmg, pending_units, pending_structs);
      else
        pending_units[tgt.unit_id] += unit.stats().dmg;
    }
  }

  for (auto& [uid, dmg] : pending_units) {
    Unit* u = world.find_unit(uid);
    if (u)
      u->hp -= dmg;
  }
  for (auto& [sid, dmg] : pending_structs) {
    Structure* s = world.find_structure(sid);
    if (s)
      s->hp -= dmg;
  }
  world.purge_dead();
}

} // namespace game
