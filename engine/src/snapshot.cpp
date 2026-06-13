#include "snapshot.hpp"
#include "world.hpp"
#include "entity.hpp"
#include "grid.hpp"
#include <vector>

namespace game {

Snapshot build_snapshot(const World& world, uint32_t faction_id) {
  FactionId my_faction{faction_id};
  const auto& res = world.res(my_faction);

  // --- Compute visibility mask -----------------------------------------
  // A tile is visible if any friendly unit's Manhattan distance to it is
  // within that unit's sight radius. No terrain blocking (LoS = radius only).
  const int32_t W = world.map.width;
  const int32_t H = world.map.height;
  std::vector<bool> visible(static_cast<size_t>(W * H), false);

  for (const auto& [id, unit] : world.units) {
    if (unit.faction != my_faction || !unit.alive())
      continue;
    const int32_t sight = unit.stats().sight;
    const int32_t x0 = std::max(0, unit.pos.x - sight);
    const int32_t x1 = std::min(W - 1, unit.pos.x + sight);
    const int32_t y0 = std::max(0, unit.pos.y - sight);
    const int32_t y1 = std::min(H - 1, unit.pos.y + sight);
    for (int32_t y = y0; y <= y1; ++y) {
      for (int32_t x = x0; x <= x1; ++x) {
        if (manhattan(unit.pos, {x, y}) <= sight)
          visible[static_cast<size_t>(y * W + x)] = true;
      }
    }
  }

  // --- Build visible-tile occupancy map --------------------------------
  // Maps tile position → occupant faction (0=none, 1=me, 2=enemy).
  std::vector<uint16_t> occupancy(static_cast<size_t>(W * H), 0);
  for (const auto& [id, unit] : world.units) {
    if (!unit.alive())
      continue;
    auto idx = static_cast<size_t>(unit.pos.y * W + unit.pos.x);
    occupancy[idx] = (unit.faction == my_faction) ? 1 : 2;
  }

  // --- Populate snapshot -----------------------------------------------
  Snapshot snap;
  snap.header.tick = world.tick;
  snap.header.my_faction_id = faction_id;
  snap.header.energy = res.energy;
  snap.header.alloy = res.alloy;
  snap.header.territory_pct = 0; // stubbed until Phase 3
  snap.header.win_threshold = 0; // stubbed until Phase 3
  snap.header.map_w = static_cast<uint32_t>(W);
  snap.header.map_h = static_cast<uint32_t>(H);

  // My units (all, regardless of fog — bots always know their own fleet)
  for (const auto& [id, unit] : world.units) {
    if (unit.faction != my_faction || !unit.alive())
      continue;
    UnitView uv{};
    uv.id = id.value;
    uv.type = static_cast<uint16_t>(unit.type);
    uv.x = unit.pos.x;
    uv.y = unit.pos.y;
    uv.hp = unit.hp;
    uv.current_order = static_cast<uint16_t>(unit.current_order);
    snap.my_units.push_back(uv);
  }

  // Visible enemies and tiles
  for (int32_t y = 0; y < H; ++y) {
    for (int32_t x = 0; x < W; ++x) {
      if (!visible[static_cast<size_t>(y * W + x)])
        continue;

      const auto& tile = world.map.tile_at({x, y});
      TileView tv{};
      tv.x = x;
      tv.y = y;
      tv.terrain = static_cast<uint16_t>(tile.terrain);
      tv.occupant_faction = occupancy[static_cast<size_t>(y * W + x)];
      tv.resource_amount = tile.resource_amount;
      snap.visible_tiles.push_back(tv);
    }
  }

  // Visible enemies: iterate units, emit if enemy and on a visible tile
  for (const auto& [id, unit] : world.units) {
    if (unit.faction == my_faction || !unit.alive())
      continue;
    if (!visible[static_cast<size_t>(unit.pos.y * W + unit.pos.x)])
      continue;
    EnemyView ev{};
    ev.id = id.value;
    ev.type = static_cast<uint16_t>(unit.type);
    ev.x = unit.pos.x;
    ev.y = unit.pos.y;
    ev.hp = unit.hp;
    snap.visible_enemies.push_back(ev);
  }

  snap.header.my_unit_count = static_cast<uint32_t>(snap.my_units.size());
  snap.header.visible_enemy_count = static_cast<uint32_t>(snap.visible_enemies.size());
  snap.header.visible_tile_count = static_cast<uint32_t>(snap.visible_tiles.size());

  // Byte offsets (for the flat WASM buffer — unused in engine-side code)
  snap.header.my_units_off = sizeof(SnapshotHeader);
  snap.header.enemies_off =
    snap.header.my_units_off + static_cast<uint32_t>(snap.my_units.size() * sizeof(UnitView));
  snap.header.tiles_off = snap.header.enemies_off +
                          static_cast<uint32_t>(snap.visible_enemies.size() * sizeof(EnemyView));

  return snap;
}

} // namespace game
