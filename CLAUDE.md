# Dev Agent Guide

Read this before touching any code. It covers what exists, what invariants must never break,
and what is explicitly out of scope.

Full game rules and ABI spec: `docs/DESIGN.md`.
Build order and acceptance criteria: `docs/IMPLEMENTATION_PLAN.md`.

---

## Current State

**Phase 1 complete.** 29/29 tests passing. The deterministic core engine is fully built.
No WASM, no visualizer, no tournament harness yet.

---

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug   # once, or when CMakeLists.txt changes
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

ASan + UBSan are enabled in Debug builds. Never suppress them — a sanitizer failure is a
determinism bug, not a false positive.

For LSP support:
```bash
ln -s build/compile_commands.json compile_commands.json
```

---

## What Is Built (engine/)

All engine source lives in `engine/src/`. Key files:

| File | Role |
|------|------|
| `abi.hpp` | **Single source of truth for all bot/engine wire structs.** `Command`, `CommandKind`, `SnapshotHeader`, `UnitView`, `EnemyView`, `TileView`. Locked with `offsetof`/`sizeof` static_asserts. Do not define ABI structs anywhere else. |
| `config.hpp` | Every balance constant and hard cap. No magic numbers anywhere else. |
| `ids.hpp` | Strong typed `UnitId`, `StructureId`, `FactionId`. Always use these; never raw `uint32_t` for ids. |
| `rng.hpp/.cpp` | xoshiro256** PRNG. Hand-rolled (not `std::mt19937`) because `std::uniform_int_distribution` is not standardized across implementations. |
| `entity.hpp` | `UnitType`, `StructureType`, `OrderType` enums; `UnitStats` table; `Unit`, `Structure`, `Resources` structs. |
| `world.hpp/.cpp` | Authoritative game state. `std::map` for units and structures (ordered, never unordered). |
| `snapshot.hpp/.cpp` | Fog-masked snapshot builder. LoS = Manhattan distance ≤ sight_radius only; no terrain blocking (decided, not deferred). |
| `command.hpp/.cpp` | `ValidatedCommands` type and `validate_commands()`. ABI structs are in `abi.hpp`, not here. |
| `movement.hpp/.cpp` | Stepped simultaneous movement. Blocked unit stays put, no retry (decided). Contested tile: higher `collision_priority` wins, ties → lower unit-id. |
| `combat.hpp/.cpp` | Sub-phase A: first-strike (Frigates, ascending unit-id, immediate deaths). Sub-phase B: simultaneous, all damage computed then applied together, splash has friendly fire. |
| `economy.hpp/.cpp` | Harvest (Drones adjacent to ResourceNode), spend, production. |
| `territory.hpp/.cpp` | **Stubbed.** Returns `TerritoryState{}` with zeros. Phase 3 work. |
| `wincheck.hpp/.cpp` | Fixed-order checks: base destruction → territory threshold → tick cap → tie-break ladder. |
| `statehash.hpp/.cpp` | Rolling xxh3 via xxhash (FetchContent, `XXH_INLINE_ALL`). Canonical field order: tick, all 4 RNG state words, draw_count, units by id, structures by id, resources. |
| `tick.hpp/.cpp` | Phase pipeline orchestrator. Two entry points: `run_tick_phases()` (caller supplies raw commands) and `run_tick()` (calls bots). |
| `replay.hpp/.cpp` | Input-log write/read. Stores raw commands per tick + initial entity placement in spawn order. Re-simulation reproduces the original hash. |
| `match.hpp/.cpp` | `run_match()`: builds replay log, calls bots, drives tick loop. |

`fixtures/idle_bot.hpp` — `IdleBot`: always returns empty commands. Used by determinism tests.

---

## Determinism Invariants — Never Violate

These silently break replay if violated. They are non-negotiable.

1. **Integer math only** in game state. No `float`/`double` touches game logic. `fixed.hpp`
   (Q32.32) is the only sanctioned exception and only where a ratio is genuinely unavoidable.
2. **Single seeded PRNG**, drawn in a fixed order. RNG draws happen in ascending unit-id order
   within the combat phase. No other entropy source anywhere.
3. **Ordered containers only.** `std::map`, never `std::unordered_map`, in any logic path.
   Iteration order of hash maps is not deterministic.
4. **All tie-breaks resolve to unit-id or structure-id**, assigned in deterministic creation
   order. Never use pointer addresses or hash values as tie-breakers.
5. **Fixed phase order every tick** (validate → move → combat → economy → spawn → territory →
   wincheck). No exceptions.
6. **Bots decide against the frozen start-of-tick snapshot.** No bot sees mid-tick state.
7. **`World::rng_seed` must be set alongside `World::rng`.** `run_match()` reads it into the
   replay log so `replay()` can reconstruct the exact same RNG state. If you forget this,
   `Determinism.ReplayReproducesHash` will fail.

Compile flags enforcing these: `-ffp-contract=off -fno-fast-math`. Set in `cmake/CompilerFlags.cmake`.

---

## ABI Rules

`abi.hpp` is the only place ABI structs live. Rules for that file:

- Fixed-width integers only (`uint32_t`, `int32_t`, `uint16_t`, `uint8_t`). No `std::` types.
- All fields naturally aligned — no `__attribute__((packed))` needed or wanted.
- Every struct has `sizeof` and `offsetof` static_asserts. If you add or change a field:
  update the asserts and bump `cfg::ABI_VERSION` in `config.hpp`.
- `CommandKind` underlying type must match `Command::kind` field type (assert enforces this).

---

## Coding Conventions

- **No comments explaining what code does.** Only add a comment when the *why* is non-obvious.
- **No magic numbers.** Every tunable goes in `config.hpp` as a named constant.
- **No bare `int` or `size_t` in state structs.** Use fixed-width types.
- **`std::map` for anything iterated in game logic.** `std::vector` is fine for local scratch.
- clang-format is configured (`.clang-format` at root). 2-space indent, 100 column limit.
  Run `clang-format -i` on files you touch.

---

## Decided Design Questions

These were explicitly decided and are not open for re-interpretation:

- **Line-of-sight:** sight radius only (Manhattan distance). No terrain blocking. Nebula
  blocking is deferred to a future phase.
- **Blocked movement:** unit stays put that step, no retry. Two units can deadlock in a
  corridor for the full movement phase — this is accepted.

---

## What Is NOT Built Yet (do not implement without being asked)

Phases 2–7 from `docs/IMPLEMENTATION_PLAN.md`:

- **Phase 2:** Replay visualizer / renderer
- **Phase 3:** Full Voronoi territory (currently stubbed), descending threshold, Claim Node fragility
- **Phase 4:** WASM bot runner (Wasmtime C API)
- **Phase 5:** C++ bot SDK, local tester, deterministic profiler
- **Phase 6:** Tournament/ladder harness, stats aggregation
- **Phase 7:** Calibration pass

Also explicitly out of v1 scope (do not build, stop and ask if it seems necessary):
cross-architecture replay, terrain-respecting territory/vision, procedural maps, 8-connectivity,
zero-copy ABI, OS-level match sandboxing, 3rd resource type, Tier-3 profiler.

---

## Running Specific Tests

```bash
# All tests
cd build && ctest --output-on-failure

# One test binary directly (shows gtest output)
./build/tests/engine_tests --gtest_filter="Determinism.*"
./build/tests/engine_tests --gtest_filter="Combat.*"
```

The determinism test suite (`Determinism.*`) must always pass. It is the keystone — if it
flakes or regresses, stop everything and fix it before proceeding.
