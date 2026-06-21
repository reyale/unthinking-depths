// Rush bot: combat units MoveAttack toward the enemy starting corner,
// drones move to the nearest visible resource node.
#include <ud/types.hpp>
#include <cstdint>

extern "C" {
  alignas(4) unsigned char SNAPSHOT_ADDR[ud::SNAPSHOT_BUFFER_SIZE];
  alignas(4) game::Command COMMAND_ADDR[game::cfg::UNIT_HARD_CAP];
}

static uint32_t g_faction  = 0;
static int32_t  g_enemy_x  = 0;
static int32_t  g_enemy_y  = 0;

static int32_t mdist(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
  int32_t dx = ax - bx; if (dx < 0) dx = -dx;
  int32_t dy = ay - by; if (dy < 0) dy = -dy;
  return dx + dy;
}

extern "C" void init(int32_t faction_id) {
  g_faction = static_cast<uint32_t>(faction_id);
  // Conservative guess; refined on first tick once map_w/map_h are known.
  g_enemy_x = (faction_id == 0) ? game::cfg::MAP_MAX_W - 3 : 2;
  g_enemy_y = (faction_id == 0) ? game::cfg::MAP_MAX_H - 3 : 2;
}

extern "C" int32_t on_tick() {
  const auto* h     = reinterpret_cast<const game::SnapshotHeader*>(SNAPSHOT_ADDR);
  const auto* units = reinterpret_cast<const game::UnitView*>(SNAPSHOT_ADDR + h->my_units_off);
  const auto* tvs   = reinterpret_cast<const game::TileView*>(SNAPSHOT_ADDR + h->tiles_off);

  if (h->map_w > 0) {
    g_enemy_x = (g_faction == 0) ? (int32_t)h->map_w - 3 : 2;
    g_enemy_y = (g_faction == 0) ? (int32_t)h->map_h - 3 : 2;
  }

  uint32_t n = 0;
  for (uint32_t i = 0; i < h->my_unit_count && n < (uint32_t)game::cfg::UNIT_HARD_CAP; ++i) {
    const game::UnitView& u = units[i];
    game::Command cmd{};
    cmd.unit_id = u.id;

    if (u.type == static_cast<uint16_t>(game::UnitType::Drone)) {
      int32_t best = 0x7fffffff, bx = -1, by = -1;
      for (uint32_t t = 0; t < h->visible_tile_count; ++t) {
        if (tvs[t].terrain != static_cast<uint16_t>(game::Terrain::ResourceNode)) continue;
        int32_t d = mdist(u.x, u.y, tvs[t].x, tvs[t].y);
        if (d < best) { best = d; bx = tvs[t].x; by = tvs[t].y; }
      }
      if (bx < 0 || best <= 1) continue;
      cmd.kind = static_cast<uint16_t>(game::CommandKind::Move);
      cmd.ax = bx; cmd.ay = by;
    } else {
      cmd.kind = static_cast<uint16_t>(game::CommandKind::MoveAttack);
      cmd.ax = g_enemy_x; cmd.ay = g_enemy_y;
    }

    COMMAND_ADDR[n++] = cmd;
  }
  return static_cast<int32_t>(n);
}
