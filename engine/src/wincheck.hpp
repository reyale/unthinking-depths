#pragma once
#include "ids.hpp"
#include "territory.hpp"
#include <cstdint>
#include <optional>

namespace game {

struct World;

enum class WinReason : uint8_t {
  BaseDestroyed,
  TerritoryThreshold,
  TickCap,
  TieBreakLadder,
  Draw, // tick cap with territory difference within DRAW_TERRITORY_MARGIN
};

struct MatchResult {
  FactionId winner;
  WinReason reason;
};

// Phase 6 win checks, in fixed order (§5 Phase 6):
//   1. Base destroyed this tick?
//   2. Territory ≥ current threshold?
//   3. Tick cap reached?
// Returns the match result, or nullopt if the game continues.
// Tie-break ladder applied when both sides qualify simultaneously.
std::optional<MatchResult> run_wincheck(const World& world, const TerritoryState& territory,
                                        uint32_t tick_cap);

} // namespace game
