#pragma once
#include "wincheck.hpp"
#include "statehash.hpp"
#include <optional>

namespace game {

struct World;
class Bot;

// Run one full tick through all six phases (§5).
// Returns a MatchResult if the game ended this tick, nullopt otherwise.
// `hash` is updated in-place after the tick resolves.
std::optional<MatchResult> run_tick(World& world, Bot& bot_a, uint32_t faction_a, Bot& bot_b,
                                    uint32_t faction_b, uint32_t tick_cap, StateHash& hash);

} // namespace game
