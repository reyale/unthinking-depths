#pragma once
#include <cstdint>
#include <vector>

// ABI structs: shared verbatim with /sdk/cpp. Fixed layout, naturally aligned,
// little-endian x86-64, fixed-width integers only.
namespace game {

struct World; // forward — full type in world.hpp

// ---- ABI structs (engine → bot) -----------------------------------------

struct SnapshotHeader {
  uint32_t tick;
  uint32_t my_faction_id;
  int32_t energy;
  int32_t alloy;
  uint32_t territory_pct; // 0..100 integer; 0 until Phase 3 (territory)
  uint32_t win_threshold; // 0..100 integer; 0 until Phase 3
  uint32_t map_w;
  uint32_t map_h;
  uint32_t my_unit_count;
  uint32_t visible_enemy_count;
  uint32_t visible_tile_count;
  // byte offsets from start of flat buffer to each array
  uint32_t my_units_off;
  uint32_t enemies_off;
  uint32_t tiles_off;
};

struct UnitView { // my units: full info
  uint32_t id;
  uint16_t type;
  uint16_t _pad;
  int32_t x, y;
  int32_t hp;
  uint16_t current_order;
  uint16_t _pad2;
};

struct EnemyView { // visible enemies: observable info only
  uint32_t id;
  uint16_t type;
  uint16_t _pad;
  int32_t x, y;
  int32_t hp;
};

struct TileView { // fog-visible tiles only
  int32_t x, y;
  uint16_t terrain;
  uint16_t occupant_faction; // 0=none, 1=me, 2=enemy
  int32_t resource_amount;
};

// ---- Engine-side snapshot (owns the data) --------------------------------

struct Snapshot {
  SnapshotHeader header{};
  std::vector<UnitView> my_units;
  std::vector<EnemyView> visible_enemies;
  std::vector<TileView> visible_tiles;
};

// ---- Builder -------------------------------------------------------------

// Build a fog-masked snapshot for `faction` from the current world state.
// LoS rule: sight radius only (Manhattan distance), no terrain blocking.
Snapshot build_snapshot(const World& world, uint32_t faction_id);

} // namespace game
