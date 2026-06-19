#include "map_io.hpp"
#include "map_validate.hpp"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "usage: sfbg_mapcheck <file.json>\n";
    return 1;
  }

  maps::MapData data;
  try {
    data = maps::load_map(argv[1]);
  } catch (const std::exception& e) {
    std::cerr << "load error: " << e.what() << "\n";
    return 1;
  }

  auto errors = maps::validate_map(data);
  if (errors.empty()) {
    std::cout << "OK: " << data.name << " (" << data.map.width << "x" << data.map.height
              << ")\n";
    return 0;
  }

  for (const auto& err : errors)
    std::cout << err << "\n";
  return 1;
}
