#pragma once
#include "ids.hpp"
#include <cstdint>
#include <map>
#include <vector>

namespace game {

// ---- ABI struct (bot → engine) -------------------------------------------

enum class CommandKind : uint16_t {
  Move = 0,
  MoveAttack = 1,
  Attack = 2,
  Gather = 3,
  Build = 4,
  DeployClaim = 5,
  Produce = 6,
};

struct Command {
  uint32_t unit_id;
  uint16_t kind;      // CommandKind
  uint16_t arg_type;  // reserved / unit_type for Produce/Build
  int32_t ax, ay;     // target tile
  uint32_t target_id; // attack target / factory id
  uint16_t aux;       // unit_type for Produce/Build
  uint16_t _pad;
};

// ---- Validated result ----------------------------------------------------

// After Phase 1 validation: at most one command per unit, keyed by UnitId.
// std::map keeps iteration order deterministic (ascending unit-id).
using ValidatedCommands = std::map<UnitId, Command>;

// ---- Validator -----------------------------------------------------------

// Filters `raw` commands against current world state for `faction_id`.
// Rules (§5 Phase 1):
//   - Unknown/dead/enemy unit → rejected
//   - Second command for same unit → first listed wins
//   - Invalid target/range/cost → rejected
// Returns one valid command per surviving unit (may be fewer than input).
ValidatedCommands validate_commands(const struct World& world, uint32_t faction_id,
                                    const std::vector<Command>& raw);

} // namespace game
