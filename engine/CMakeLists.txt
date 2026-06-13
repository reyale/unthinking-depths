add_library(engine STATIC
    src/fixed.cpp
    src/rng.cpp
    src/grid.cpp
    src/world.cpp
    src/snapshot.cpp
    src/command.cpp
    src/movement.cpp
    src/combat.cpp
    src/economy.cpp
    src/territory.cpp
    src/wincheck.cpp
    src/statehash.cpp
    src/tick.cpp
    src/replay.cpp
    src/match.cpp
)

target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(engine PUBLIC xxhash_iface)

