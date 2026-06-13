#pragma once
#include "abi.hpp"
#include "ids.hpp"
#include <map>
#include <vector>

namespace game {

// ---- Validated result -------------------------------------------------------

// After Phase 1 validation: at most one command per unit, keyed by UnitId.
// std::map keeps iteration order deterministic (ascending unit-id).
using ValidatedCommands = std::map<UnitId, Command>;

// ---- Validator --------------------------------------------------------------

// Filters `raw` commands against current world state for `faction_id`.
// Rules (§5 Phase 1):
//   - Unknown/dead/enemy unit → rejected
//   - Second command for same unit → first listed wins
//   - Invalid target/range/cost → rejected
// Returns one valid command per surviving unit (may be fewer than input).
ValidatedCommands validate_commands(const struct World& world, uint32_t faction_id,
                                    const std::vector<Command>& raw);

} // namespace game
