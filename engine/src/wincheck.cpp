#include "wincheck.hpp"
#include "world.hpp"
#include "entity.hpp"
#include "config.hpp"

namespace game {

// Tie-break ladder (§7.4): evaluated in order until one side wins.
static FactionId tiebreak(const World& world, const TerritoryState& territory) {
  FactionId f0{0}, f1{1};

  // 1. Higher territory %
  if (territory.pct_faction[0] != territory.pct_faction[1])
    return territory.pct_faction[0] > territory.pct_faction[1] ? f0 : f1;

  // 2. Higher Command Core hp remaining
  int32_t hp0 = 0, hp1 = 0;
  for (const auto& [sid, s] : world.structures) {
    if (s.type != StructureType::CommandCore || !s.alive())
      continue;
    if (s.faction.value == 0)
      hp0 = s.hp;
    else
      hp1 = s.hp;
  }
  if (hp0 != hp1)
    return hp0 > hp1 ? f0 : f1;

  // 3–5 require per-match stats tracking (deferred to Phase 6).
  // 6. Deterministic coin-flip: lower faction id wins.
  return f0;
}

std::optional<MatchResult> run_wincheck(const World& world, const TerritoryState& territory,
                                        uint32_t tick_cap) {
  FactionId f0{0}, f1{1};

  bool core0_alive = world.command_core(f0) != nullptr;
  bool core1_alive = world.command_core(f1) != nullptr;

  // 1. Base destroyed?
  if (!core0_alive || !core1_alive) {
    if (!core0_alive && !core1_alive)
      return MatchResult{FactionId{}, WinReason::Draw};
    return MatchResult{core0_alive ? f0 : f1, WinReason::BaseDestroyed};
  }

  // 2. Territory ≥ threshold?
  int32_t threshold = win_threshold(world.tick, tick_cap);
  bool t0 = static_cast<int32_t>(territory.pct_faction[0]) >= threshold;
  bool t1 = static_cast<int32_t>(territory.pct_faction[1]) >= threshold;
  if (t0 || t1) {
    if (t0 && t1)
      return MatchResult{tiebreak(world, territory), WinReason::TieBreakLadder};
    return MatchResult{t0 ? f0 : f1, WinReason::TerritoryThreshold};
  }

  // 3. Tick cap?
  if (world.tick >= tick_cap) {
    int32_t diff = static_cast<int32_t>(territory.pct_faction[0]) -
                   static_cast<int32_t>(territory.pct_faction[1]);
    if (diff < 0) diff = -diff;
    if (diff <= cfg::DRAW_TERRITORY_MARGIN)
      return MatchResult{FactionId{}, WinReason::Draw};
    return MatchResult{territory.pct_faction[0] > territory.pct_faction[1] ? f0 : f1,
                       WinReason::TickCap};
  }

  return std::nullopt;
}

} // namespace game
