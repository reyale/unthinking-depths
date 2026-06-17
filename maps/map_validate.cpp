#include "map_validate.hpp"
#include "config.hpp"
#include <queue>
#include <vector>
#include <string>

namespace maps {

std::vector<std::string> validate_map(const MapData& data) {
  std::vector<std::string> errors;

  int32_t w = data.map.width;
  int32_t h = data.map.height;

  if (w < 4 || w > game::cfg::MAP_MAX_W || h < 4 || h > game::cfg::MAP_MAX_H) {
    errors.push_back(
        "dimensions " + std::to_string(w) + "x" + std::to_string(h) +
        " out of range [4," + std::to_string(game::cfg::MAP_MAX_W) + "] x [4," +
        std::to_string(game::cfg::MAP_MAX_H) + "]");
    return errors;
  }

  for (int i = 0; i < 2; ++i) {
    game::Vec2 sp = data.spawn[i];
    if (!data.map.passable(sp)) {
      errors.push_back(
          "spawn[" + std::to_string(i) + "] at (" + std::to_string(sp.x) + "," +
          std::to_string(sp.y) + ") is not passable or out of bounds");
    }
  }

  [&]() {
    for (int32_t y = 0; y < h; ++y) {
      for (int32_t x = 0; x < w; ++x) {
        game::Vec2 p{x, y};
        game::Vec2 mirror{w - 1 - x, h - 1 - y};
        if (data.map.tile_at(p).terrain != data.map.tile_at(mirror).terrain) {
          errors.push_back(
              "map is not point-symmetric: tile at (" + std::to_string(x) + "," +
              std::to_string(y) + ") != tile at (" + std::to_string(mirror.x) + "," +
              std::to_string(mirror.y) + ")");
          return;
        }
      }
    }
  }();

  {
    game::Vec2 expected_mirror{w - 1 - data.spawn[0].x, h - 1 - data.spawn[0].y};
    if (data.spawn[1].x != expected_mirror.x || data.spawn[1].y != expected_mirror.y) {
      errors.push_back(
          "spawn symmetry violated: spawn[1] (" + std::to_string(data.spawn[1].x) + "," +
          std::to_string(data.spawn[1].y) + ") is not the 180-degree mirror of spawn[0] (" +
          std::to_string(data.spawn[0].x) + "," + std::to_string(data.spawn[0].y) + ")");
    }
  }

  auto run_connectivity = [&]() {
    if (!data.map.passable(data.spawn[0]))
      return;

    int32_t total = w * h;
    std::vector<bool> visited(static_cast<size_t>(total), false);

    auto idx = [&](game::Vec2 p) {
      return static_cast<size_t>(p.y) * static_cast<size_t>(w) + static_cast<size_t>(p.x);
    };

    std::queue<game::Vec2> q;
    q.push(data.spawn[0]);
    visited[idx(data.spawn[0])] = true;

    bool spawn1_reached = false;
    bool resource_reached = false;

    while (!q.empty()) {
      game::Vec2 cur = q.front();
      q.pop();

      if (cur == data.spawn[1])
        spawn1_reached = true;
      if (data.map.in_bounds(cur) &&
          data.map.tile_at(cur).terrain == game::Terrain::ResourceNode)
        resource_reached = true;

      for (auto d : game::DIRS) {
        game::Vec2 nb = cur + d;
        if (!data.map.passable(nb))
          continue;
        if (visited[idx(nb)])
          continue;
        visited[idx(nb)] = true;
        q.push(nb);
      }
    }

    if (!spawn1_reached)
      errors.push_back("spawn[1] is not reachable from spawn[0]");
    if (!resource_reached)
      errors.push_back("no ResourceNode is reachable from spawn[0]");

    for (int32_t y = 0; y < h; ++y) {
      for (int32_t x = 0; x < w; ++x) {
        game::Vec2 p{x, y};
        if (data.map.passable(p) && !visited[idx(p)]) {
          errors.push_back(
              "isolated passable tile at (" + std::to_string(x) + "," + std::to_string(y) +
              ") is not reachable from spawn[0]");
          return;
        }
      }
    }
  };
  run_connectivity();

  bool any_resource = false;
  for (int32_t y = 0; y < h; ++y) {
    for (int32_t x = 0; x < w; ++x) {
      if (data.map.tile_at({x, y}).terrain == game::Terrain::ResourceNode) {
        any_resource = true;
        break;
      }
    }
    if (any_resource)
      break;
  }
  if (!any_resource)
    errors.push_back("map has no ResourceNode tiles");

  return errors;
}

} // namespace maps
