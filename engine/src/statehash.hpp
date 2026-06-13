#pragma once
#include <cstdint>
#include <vector>

namespace game {

struct World;

// Rolling xxh3 digest over canonical game state.
// Canonical field order: tick, rng draw_count, then units by ascending id
// (id, type, pos, hp, order), then structures by ascending id (id, type, pos, hp),
// then per-faction resources.
// Per-tick digests are retained so divergence is binary-searchable to the exact tick.
struct StateHash {
  uint64_t current{0};            // rolling hash after each tick
  std::vector<uint64_t> per_tick; // per_tick[t] = hash after tick t

  // Feed the current world state into the rolling digest and record it.
  void update(const World& world);

  // Final match fingerprint (last recorded digest).
  uint64_t fingerprint() const;
};

} // namespace game
