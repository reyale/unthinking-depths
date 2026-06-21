#pragma once
#include <cstddef>
#include <cstdint>

namespace game {

enum class UnitType : uint8_t; // full definition in entity.hpp

struct UnitStats {
  int32_t hp;
  int32_t dmg;
  int32_t range;
  int32_t sight;
  int32_t speed;
  bool first_strike;
  bool splash;
  int32_t splash_radius;
  int32_t collision_priority;
};

const UnitStats& stats_for(UnitType t);

} // namespace game
