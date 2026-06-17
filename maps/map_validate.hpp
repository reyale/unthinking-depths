#pragma once
#include "map_io.hpp"
#include <string>
#include <vector>

namespace maps {
// Returns empty vector if valid; otherwise one string per error.
std::vector<std::string> validate_map(const MapData& data);
} // namespace maps
