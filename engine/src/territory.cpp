#include "territory.hpp"
#include "world.hpp"
#include "config.hpp"

namespace game {

// Phase 3 stub — full Voronoi implementation deferred.
TerritoryState run_territory(World& /*world*/) {
  return TerritoryState{};
}

int32_t win_threshold(uint32_t tick, uint32_t tick_cap) {
  if (tick_cap == 0)
    return cfg::THRESH_A;
  // g^2 in integer: scale by 10000 to avoid fixed-point for this simple formula
  int64_t g2 =
    (static_cast<int64_t>(tick) * tick) / (static_cast<int64_t>(tick_cap) * tick_cap / 10000);
  int32_t drop = static_cast<int32_t>(static_cast<int64_t>(cfg::THRESH_B) * g2 / 10000);
  int32_t threshold = cfg::THRESH_A - drop;
  if (threshold < cfg::THRESH_MIN)
    threshold = cfg::THRESH_MIN;
  return threshold;
}

} // namespace game
