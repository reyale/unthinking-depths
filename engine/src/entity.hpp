#pragma once
#include "ids.hpp"
#include "grid.hpp"
#include "config.hpp"
#include <cstdint>
#include <optional>
#include <type_traits>

namespace game {

// ---- Enums ----------------------------------------------------------------

enum class UnitType : uint8_t {
  Drone = 0,
  Interceptor = 1,
  Frigate = 2,
  Artillery = 3,
};
static_assert(std::is_same_v<std::underlying_type_t<UnitType>, uint8_t>,
              "UnitType underlying type changed — update Command::aux consumers");

enum class StructureType : uint8_t {
  CommandCore = 0,
  Factory = 1,
  ClaimNode = 2,
};
static_assert(std::is_same_v<std::underlying_type_t<StructureType>, uint8_t>,
              "StructureType underlying type changed — update Command::aux consumers");

enum class OrderType : uint8_t {
  Idle = 0,
  Move = 1,
  MoveAttack = 2,
  Attack = 3,
  Gather = 4,
  Build = 5,
  DeployClaim = 6,
};

// ---- Stat tables ----------------------------------------------------------

struct UnitStats {
  int32_t hp;
  int32_t dmg;
  int32_t range;
  int32_t sight;
  int32_t speed;
  bool first_strike;
  bool splash; // radius-1 splash (Artillery)
  int32_t splash_radius;
  int32_t collision_priority; // higher wins contested tile
};

constexpr UnitStats UNIT_STATS[] = {
  // Drone
  {cfg::DRONE_HP, 0, 0, cfg::DRONE_SIGHT, cfg::DRONE_SPEED, false, false, 0, /*priority*/ 1},
  // Interceptor
  {cfg::INTERCEPTOR_HP, cfg::INTERCEPTOR_DMG, cfg::INTERCEPTOR_RANGE, cfg::INTERCEPTOR_SIGHT,
   cfg::INTERCEPTOR_SPEED, false, false, 0, /*priority*/ 3},
  // Frigate
  {cfg::FRIGATE_HP, cfg::FRIGATE_DMG, cfg::FRIGATE_RANGE, cfg::FRIGATE_SIGHT, cfg::FRIGATE_SPEED,
   /*first_strike*/ true, false, 0, /*priority*/ 1},
  // Artillery
  {cfg::ARTILLERY_HP, cfg::ARTILLERY_DMG, cfg::ARTILLERY_RANGE, cfg::ARTILLERY_SIGHT,
   cfg::ARTILLERY_SPEED, false, /*splash*/ true, cfg::ARTILLERY_SPLASH_RADIUS, /*priority*/ 1},
};

constexpr const UnitStats& stats_for(UnitType t) {
  return UNIT_STATS[static_cast<uint8_t>(t)];
}

// ---- Unit -----------------------------------------------------------------

struct Unit {
  UnitId id;
  FactionId faction;
  UnitType type;
  Vec2 pos;
  int32_t hp;

  OrderType current_order{OrderType::Idle};
  Vec2 order_target{};    // tile target for Move/MoveAttack/Attack
  UnitId attack_target{}; // explicit attack target (may be null)

  int32_t move_steps_remaining{0}; // steps left this tick's movement phase

  bool alive() const { return hp > 0; }
  const UnitStats& stats() const { return stats_for(type); }
};

// ---- Structure ------------------------------------------------------------

struct Structure {
  StructureId id;
  FactionId faction;
  StructureType type;
  Vec2 pos;
  int32_t hp;

  // Factory: production queue (single slot, v1)
  std::optional<UnitType> production_queue{};
  int32_t production_ticks_left{0};

  bool alive() const { return hp > 0; }
};

// ---- Resources per faction ------------------------------------------------

struct Resources {
  int32_t energy{0};
  int32_t alloy{0};
};

} // namespace game
