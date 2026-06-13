#include "fixed.hpp"
#include <cmath> // std::sqrt — the ONLY place floating-point is permitted in the engine

// __int128 is a GCC/Clang extension needed for 64×64 fixed-point multiply.
#pragma GCC diagnostic ignored "-Wpedantic"

namespace game {

Fixed isqrt(int64_t n) {
  if (n <= 0)
    return Fixed{};
  // Float gives the initial approximation; immediately cast to Q32.32.
  auto approx =
    static_cast<int64_t>(std::sqrt(static_cast<double>(n)) * static_cast<double>(Fixed::ONE));
  // One Newton refinement in Q32.32: x1 = (x0 + (n_q / x0)) / 2
  // where n_q = n in Q32.32 (n << FRAC_BITS).
  if (approx > 0) {
    auto n_q = static_cast<__int128>(n) << Fixed::FRAC_BITS;
    approx = (approx + static_cast<int64_t>(n_q / approx)) / 2;
  }
  return Fixed::from_raw(approx);
}

} // namespace game
