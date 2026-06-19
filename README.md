# Space Fleet Battle Game

A deterministic, server-executed strategy game where programmers submit bots that command autonomous space fleets in 1v1 head-to-head matches.

Each bot compiles to WASM and receives a fog-masked snapshot of the battlefield each tick, returning commands for its entire fleet. Matches run centrally, are fully replayable, and are designed to be watchable.

## How it works

- Two bots control fleets on a shared grid
- Each tick, bots get a fog-of-war snapshot and return commands — one call for the whole fleet ("army-brain" control)
- The simulation is fully deterministic: the same map, seed, and bot binaries always produce the same match
- Matches produce a replay log; a renderer plays it back with scrub, pause, and perspective toggle

## Unit roster

| Unit | Role | Special |
|------|------|---------|
| Drone | Economy | Harvests resources, builds structures |
| Interceptor | Brawler | Fast, high collision priority |
| Frigate | Ranged | First-strike; fires before the simultaneous phase |
| Artillery | Siege | Radius splash with friendly fire; cracks Claim Nodes |

Counter-triangle: Interceptor > Frigate > Artillery > Interceptor.

## Win conditions

1. **Base destruction** — destroy the enemy Command Core
2. **Territory control** — hold ≥ a descending threshold of Voronoi territory (starts ~78%, falls to ~51% at the tick cap), anchored by Claim Nodes

The threshold falls over time so turtling loses. Draws are essentially impossible.

## Bot API

```cpp
void on_init(MapInfo);                        // one-time setup, generous fuel budget
std::vector<Command> on_tick(WorldView);      // per tick, metered fuel budget
```

Bots compile to `wasm32` via any LLVM toolchain (Rust, C++, Zig, AssemblyScript, ...). Fuel is deterministic — authors get exact, reproducible gas/memory reports before submitting.

## Repository layout

```
engine/       Pure deterministic simulation — no I/O, no WASM in the hot path
runner/       Wasmtime host; wraps the engine for untrusted .wasm bots
sdk/cpp/      Bot template, ABI headers, local tester
profiler/     Scenario suite — exact fuel/memory reports pre-match
harness/      Tournament/ladder runner and stats aggregation
viz/          Replay renderer (re-simulates via engine)
maps/         Hand-authored point-symmetric maps
docs/         Design doc and implementation plan
tests/        Unit and golden-replay tests
```

## Building

Requires GCC 14+, CMake 3.16+.

```bash
# Configure (once, or when CMakeLists changes)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Test
cd build && ctest --output-on-failure
```

## Coverage

Requires `gcovr` (`pip install gcovr`).

```bash
# Configure a separate coverage build (once)
cmake -B build-cov -DCMAKE_BUILD_TYPE=Coverage

# Build and run — produces HTML + XML reports
cmake --build build-cov --target coverage

# Open the report
xdg-open build-cov/coverage/index.html
```

For editor LSP support, symlink the compile commands:

```bash
ln -s build/compile_commands.json compile_commands.json
```

## Design and implementation docs

- [docs/DESIGN.md](docs/DESIGN.md) — full game rules, tick resolution, win conditions, unit roster, ABI contract
- [docs/IMPLEMENTATION_PLAN.md](docs/IMPLEMENTATION_PLAN.md) — build phases, technical decisions, acceptance criteria
