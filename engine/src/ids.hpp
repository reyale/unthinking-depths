#pragma once
#include <cstdint>
#include <compare>
#include <limits>

// Strong-typed ids assigned in deterministic creation order.
// All tie-breaks must resolve to id (ascending), never pointer or hash-map order.
namespace game {

template <typename Tag>
struct StrongId {
  uint32_t value{std::numeric_limits<uint32_t>::max()};

  constexpr bool is_null() const { return value == std::numeric_limits<uint32_t>::max(); }
  auto operator<=>(const StrongId&) const = default;
};

struct UnitTag {};
struct StructureTag {};
struct FactionTag {};

using UnitId = StrongId<UnitTag>;
using StructureId = StrongId<StructureTag>;
using FactionId = StrongId<FactionTag>;

} // namespace game
