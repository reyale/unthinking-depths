#pragma once
#include "command.hpp"

namespace game {

struct World;

// Phase 4 — economy.
// Drones on or adjacent to ResourceNodes credit energy/alloy.
// Build/Produce/DeployClaim costs are deducted in deterministic id order;
// insufficient resources → action silently fails (logged in future).
void run_economy(World& world, const ValidatedCommands& cmds_a, const ValidatedCommands& cmds_b);

} // namespace game
