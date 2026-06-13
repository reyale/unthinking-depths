#pragma once
#include "command.hpp"

namespace game {

struct World;

// Phase 3 — combat against final positions.
//
// Sub-phase A (first-strike): Frigates fire first, iterating in ascending
//   unit-id order, drawing from the world RNG in that order.
//   Targets reduced to ≤0 hp are destroyed before sub-phase B.
//
// Sub-phase B (simultaneous): all other attackers (Interceptor, Artillery)
//   compute damage against survivors, then apply it all at once.
//   Mutual kills both die. Artillery splash hits a radius-1 area, friendly
//   fire included, all simultaneous.
//
// Targeting: explicit target if valid and in range; else nearest enemy in
//   range (Manhattan); else no attack.
void run_combat(World& world, const ValidatedCommands& cmds_a, const ValidatedCommands& cmds_b);

} // namespace game
