#include "match.hpp"
#include "bot_iface.hpp"
#include "config.hpp"
#include "grid.hpp"
#include "snapshot.hpp"
#include "tick.hpp"
#include "world.hpp"

namespace game {

MatchRecord run_match(World& world, Bot& bot_a, uint32_t faction_a, Bot& bot_b, uint32_t faction_b,
                      uint32_t tick_cap) {
  MatchRecord rec;

  // Populate replay header
  rec.replay.abi_version = cfg::ABI_VERSION;
  rec.replay.seed = world.rng_seed;
  rec.replay.map_w = world.map.width;
  rec.replay.map_h = world.map.height;
  rec.replay.faction_a = faction_a;
  rec.replay.faction_b = faction_b;
  rec.replay.tick_cap = tick_cap;

  // Snapshot map tiles into the replay log
  rec.replay.map_tiles.reserve(static_cast<size_t>(world.map.width) *
                               static_cast<size_t>(world.map.height));
  for (int32_t y = 0; y < world.map.height; ++y)
    for (int32_t x = 0; x < world.map.width; ++x)
      rec.replay.map_tiles.push_back(world.map.tile_at({x, y}));

  // Record initial entity placement in id order (= spawn order)
  for (const auto& [uid, u] : world.units)
    rec.replay.initial_entities.push_back(
      {u.faction.value, static_cast<uint8_t>(u.type), false, u.pos.x, u.pos.y});
  for (const auto& [sid, s] : world.structures)
    rec.replay.initial_entities.push_back(
      {s.faction.value, static_cast<uint8_t>(s.type), true, s.pos.x, s.pos.y});

  // Init bots
  bot_a.on_init(world.map, faction_a);
  bot_b.on_init(world.map, faction_b);

  // Tick loop
  std::optional<MatchResult> result;
  while (!result) {
    // Phase 0 — build snapshots and call bots
    Snapshot snap_a = build_snapshot(world, faction_a);
    Snapshot snap_b = build_snapshot(world, faction_b);

    std::vector<Command> raw_a = bot_a.healthy() ? bot_a.on_tick(snap_a) : std::vector<Command>{};
    std::vector<Command> raw_b = bot_b.healthy() ? bot_b.on_tick(snap_b) : std::vector<Command>{};

    // Record before phases consume/validate them
    rec.replay.ticks.push_back({world.tick, raw_a, raw_b});

    // Phases 1–6
    result = run_tick_phases(world, raw_a, faction_a, raw_b, faction_b, tick_cap, rec.hash);
  }

  rec.outcome = *result;
  rec.ticks_played = world.tick;
  rec.replay.expected_hash = rec.hash.fingerprint();
  rec.replay.outcome = *result;

  return rec;
}

} // namespace game
