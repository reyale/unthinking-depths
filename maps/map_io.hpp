#pragma once
#include "grid.hpp"
#include <string>
#include <vector>

namespace maps {

struct MapData {
  std::string name;
  int32_t     resource_amount{500};
  game::Map   map;
  game::Vec2  spawn[2]; // spawn[0]='1', spawn[1]='2' in the ASCII grid
  uint64_t    hash{0};  // xxh3-64 over map content; populated by load_map/save_map
};

// ASCII character encoding (documented here for tool authors):
//   '.' = Open   '#' = Asteroid   '~' = Nebula   'R' = ResourceNode
//   '1' = spawn[0] (Open terrain at runtime)
//   '2' = spawn[1] (Open terrain at runtime)

// Compute xxh3-64 over map content (dimensions, spawns, resource_amount, tile terrain).
// Name is excluded — the hash identifies layout, not metadata.
uint64_t compute_map_hash(const MapData& data);

MapData load_map(const std::string& path);
void    save_map(const MapData& data, const std::string& path);

} // namespace maps
