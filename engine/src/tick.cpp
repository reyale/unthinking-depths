#include "tick.hpp"
#include "world.hpp"
#include "bot_iface.hpp"
#include "snapshot.hpp"
#include "command.hpp"
#include "movement.hpp"
#include "combat.hpp"
#include "economy.hpp"
#include "territory.hpp"
#include "wincheck.hpp"
#include "statehash.hpp"

namespace game {

std::optional<MatchResult> run_tick(World& world, Bot& bot_a, uint32_t faction_a, Bot& bot_b,
                                    uint32_t faction_b, uint32_t tick_cap, StateHash& hash) {
  // Phase 0 — Snapshot & Decide
  // Build fog-masked snapshots from the frozen start-of-tick state.
  Snapshot snap_a = build_snapshot(world, faction_a);
  Snapshot snap_b = build_snapshot(world, faction_b);

  // Both bots decide. Unhealthy bot → empty command list (forfeit).
  std::vector<Command> raw_a = bot_a.healthy() ? bot_a.on_tick(snap_a) : std::vector<Command>{};
  std::vector<Command> raw_b = bot_b.healthy() ? bot_b.on_tick(snap_b) : std::vector<Command>{};

  // Phase 1 — Validate
  ValidatedCommands cmds_a = validate_commands(world, faction_a, raw_a);
  ValidatedCommands cmds_b = validate_commands(world, faction_b, raw_b);

  // Phase 2 — Movement
  run_movement(world, cmds_a, cmds_b);

  // Phase 3 — Combat
  run_combat(world, cmds_a, cmds_b);

  // Phase 4 — Economy
  run_economy(world, cmds_a, cmds_b);

  // Phase 5 — Spawn / Upkeep (production is handled inside economy for now)

  // Phase 6 — Territory & Win Check
  TerritoryState territory = run_territory(world);
  ++world.tick;

  auto result = run_wincheck(world, territory, tick_cap);

  // Update rolling hash after the tick fully resolves
  hash.update(world);

  return result;
}

} // namespace game
