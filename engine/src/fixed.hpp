#pragma once
#include <compare>
#include <cstdint>

// __int128 is a GCC/Clang extension; suppress the pedantic warning site-wide
// for this header since it's unavoidable for 64×64 fixed-point multiply.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

// Q32.32 fixed-point in int64_t.
// Sanctioned only where integer arithmetic genuinely won't do — the one known
// case is Euclidean distance in the Voronoi tie-break. Game state math is
// otherwise pure integer. No float/double ever enters game state.
namespace game {

struct Fixed {
  static constexpr int FRAC_BITS = 32;
  static constexpr int64_t ONE = int64_t{1} << FRAC_BITS;

  int64_t raw{0};

  static constexpr Fixed from_int(int32_t v) { return {int64_t{v} << FRAC_BITS}; }
  static constexpr Fixed from_raw(int64_t r) { return {r}; }

  constexpr int32_t floor() const { return static_cast<int32_t>(raw >> FRAC_BITS); }
  constexpr int32_t round() const { return static_cast<int32_t>((raw + (ONE >> 1)) >> FRAC_BITS); }

  constexpr Fixed operator+(Fixed o) const { return {raw + o.raw}; }
  constexpr Fixed operator-(Fixed o) const { return {raw - o.raw}; }

  constexpr Fixed operator*(Fixed o) const {
    return {static_cast<int64_t>((__int128{raw} * o.raw) >> FRAC_BITS)};
  }

  auto operator<=>(const Fixed&) const = default;
};

// Integer square root returned as Fixed. The only function that touches
// floating-point; result is immediately fixed-point and never stored in state.
Fixed isqrt(int64_t n);

} // namespace game

#pragma GCC diagnostic pop
