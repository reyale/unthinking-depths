#include <gtest/gtest.h>
#include "config.hpp"
#include "match.hpp"
#include "world.hpp"
#include "wasm_bot.hpp"
#include "../../fixtures/idle_bot.hpp"

// An idle WASM bot: exports the required ABI surface, always returns 0 commands.
//
// Memory layout:
//   pages 0-4 (320 KB): snapshot buffer (SNAPSHOT_ADDR = 0)
//   page 5    ( 64 KB): command buffer  (COMMAND_ADDR  = 327680)
// This covers the worst-case snapshot size for v1 caps.
static constexpr std::string_view IDLE_WAT = R"wat(
(module
  (memory (export "memory") 6)
  (global (export "SNAPSHOT_ADDR") i32 (i32.const 0))
  (global (export "COMMAND_ADDR")  i32 (i32.const 327680))
  (func   (export "init")    (param i32))
  (func   (export "on_tick") (result i32) i32.const 0)
)
)wat";

// Minimal WAT with all required exports but only 1 page (64 KB) — intentionally
// too small for the worst-case snapshot so load validation rejects it.
static constexpr std::string_view TOO_SMALL_WAT = R"wat(
(module
  (memory (export "memory") 1)
  (global (export "SNAPSHOT_ADDR") i32 (i32.const 0))
  (global (export "COMMAND_ADDR")  i32 (i32.const 60000))
  (func   (export "init")    (param i32))
  (func   (export "on_tick") (result i32) i32.const 0)
)
)wat";

// WAT bot that burns all its fuel in a tight loop during init.
static constexpr std::string_view FUEL_HOG_INIT_WAT = R"wat(
(module
  (memory (export "memory") 6)
  (global (export "SNAPSHOT_ADDR") i32 (i32.const 0))
  (global (export "COMMAND_ADDR")  i32 (i32.const 327680))
  (func (export "init") (param i32)
    (block $break
      (loop $loop
        br $loop)))
  (func (export "on_tick") (result i32) i32.const 0)
)
)wat";

// ---- Helper -----------------------------------------------------------------

static game::World make_world(uint64_t seed = 42) {
  game::World w;
  w.map = game::Map::make(20, 20);
  w.rng = game::Rng{seed};
  w.rng_seed = seed;
  w.spawn_structure({0}, game::StructureType::CommandCore, {2, 2});
  w.spawn_unit({0}, game::UnitType::Drone, {3, 2});
  w.spawn_structure({1}, game::StructureType::CommandCore, {17, 17});
  w.spawn_unit({1}, game::UnitType::Drone, {16, 17});
  return w;
}

// ---- Tests ------------------------------------------------------------------

TEST(WasmBot, IdleBotLoads) {
  auto wasm = runner::wat_to_wasm(IDLE_WAT);
  runner::WasmBot bot(wasm);
  EXPECT_TRUE(bot.healthy());
  EXPECT_TRUE(bot.last_error().empty());
}

TEST(WasmBot, IdleBotRunsFullMatch) {
  auto wasm = runner::wat_to_wasm(IDLE_WAT);
  runner::WasmBot bot_a(wasm);
  runner::WasmBot bot_b(runner::wat_to_wasm(IDLE_WAT));

  auto w = make_world(7);
  auto rec = game::run_match(w, bot_a, 0, bot_b, 1, 200);

  EXPECT_TRUE(bot_a.healthy());
  EXPECT_TRUE(bot_b.healthy());
  // Both bots idle → tick cap reached.
  EXPECT_EQ(rec.outcome.reason, game::WinReason::TickCap);
}

TEST(WasmBot, WasmMatchReproducesHash) {
  auto wasm = runner::wat_to_wasm(IDLE_WAT);
  auto w = make_world(99);
  runner::WasmBot bot_a(wasm);
  runner::WasmBot bot_b(runner::wat_to_wasm(IDLE_WAT));
  auto rec = game::run_match(w, bot_a, 0, bot_b, 1, 200);

  uint64_t replayed = game::replay(rec.replay);
  EXPECT_EQ(replayed, rec.replay.expected_hash);
}

TEST(WasmBot, WasmAndScriptedBotProduceSameHash) {
  // An idle WASM bot and our C++ IdleBot are semantically identical.
  // Running the same seed through both should produce the same final hash.
  auto wasm = runner::wat_to_wasm(IDLE_WAT);

  auto run_wasm = [&](uint64_t seed) {
    auto w = make_world(seed);
    runner::WasmBot a(wasm), b(runner::wat_to_wasm(IDLE_WAT));
    return game::run_match(w, a, 0, b, 1, 200).replay.expected_hash;
  };

  auto run_scripted = [](uint64_t seed) {
    auto w = make_world(seed);
    game::IdleBot a, b;
    return game::run_match(w, a, 0, b, 1, 200).replay.expected_hash;
  };

  for (uint64_t seed : {1ULL, 42ULL, 99ULL}) {
    EXPECT_EQ(run_wasm(seed), run_scripted(seed))
        << "hash mismatch at seed " << seed;
  }
}

TEST(WasmBot, TooSmallMemoryRejectedAtLoad) {
  auto wasm = runner::wat_to_wasm(TOO_SMALL_WAT);
  runner::WasmBot bot(wasm);
  EXPECT_FALSE(bot.healthy());
  EXPECT_FALSE(bot.last_error().empty());
}

TEST(WasmBot, MissingExportRejectedAtLoad) {
  // A module with no exports at all.
  auto wasm = runner::wat_to_wasm("(module (memory 6))");
  runner::WasmBot bot(wasm);
  EXPECT_FALSE(bot.healthy());
}

TEST(WasmBot, InitFuelExhaustionMarksUnhealthy) {
  auto wasm = runner::wat_to_wasm(FUEL_HOG_INIT_WAT);
  runner::WasmBot bot(wasm);
  ASSERT_TRUE(bot.healthy()); // loads fine

  auto w = make_world();
  bot.on_init(w.map, 0); // burns all init fuel

  EXPECT_FALSE(bot.healthy());
  EXPECT_FALSE(bot.last_error().empty());
}

TEST(WasmBot, DeterministicAcrossRuns) {
  auto wasm = runner::wat_to_wasm(IDLE_WAT);

  auto run = [&](uint64_t seed) {
    auto w = make_world(seed);
    runner::WasmBot a(wasm), b(runner::wat_to_wasm(IDLE_WAT));
    return game::run_match(w, a, 0, b, 1, 100).replay.expected_hash;
  };

  for (uint64_t seed : {5ULL, 17ULL, 31ULL}) {
    EXPECT_EQ(run(seed), run(seed)) << "non-deterministic at seed " << seed;
  }
}
