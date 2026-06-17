#pragma once
#include "map_io.hpp"
#include "rng.hpp"
#include <cstdint>
#include <string>

namespace maps {

struct GenParams {
  int32_t     width{30};
  int32_t     height{20};
  int32_t     resource_amount{500};
  std::string name{"generated"};
};

// Generates a random point-symmetric map. Uses rng for all randomness (rng
// state advances each attempt so repeated calls produce distinct maps).
// Throws std::runtime_error if no valid map is found within max_attempts.
MapData generate_map(const GenParams& p, game::Rng& rng, int max_attempts = 30);

} // namespace maps
