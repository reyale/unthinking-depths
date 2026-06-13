#pragma once
#include "replay.hpp"
#include "statehash.hpp"
#include "wincheck.hpp"
#include <cstdint>

namespace game {

struct World;
class Bot;

struct MatchRecord {
  MatchResult outcome;
  uint32_t ticks_played;
  StateHash hash;   // per-tick digests retained for binary-search divergence
  ReplayLog replay; // input-log; re-simulate to verify
};

// Run a full match on a pre-configured world (units and structures already placed).
// Records every tick's raw commands into a ReplayLog so the match can be
// reproduced exactly via replay().
MatchRecord run_match(World& world, Bot& bot_a, uint32_t faction_a, Bot& bot_b, uint32_t faction_b,
                      uint32_t tick_cap);

} // namespace game
