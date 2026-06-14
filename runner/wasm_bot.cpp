#include "wasm_bot.hpp"
#include "abi.hpp"
#include "config.hpp"
#include <wasmtime.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace runner {

// ---- Helpers ----------------------------------------------------------------

// Drain a wasmtime_error_t or wasm_trap_t into a string, then free both.
static std::string drain_error(wasmtime_error_t* err, wasm_trap_t* trap) {
  wasm_byte_vec_t msg;
  wasm_byte_vec_new_empty(&msg);
  if (err) {
    wasmtime_error_message(err, &msg);
    wasmtime_error_delete(err);
  } else if (trap) {
    wasm_trap_message(trap, &msg);
    wasm_trap_delete(trap);
  }
  std::string s(msg.data, msg.size);
  wasm_byte_vec_delete(&msg);
  return s;
}

// Maximum sizes for validation at load time.
static constexpr size_t MAX_SNAPSHOT_BYTES =
    sizeof(game::SnapshotHeader) +
    game::cfg::UNIT_HARD_CAP * sizeof(game::UnitView) +
    game::cfg::UNIT_HARD_CAP * 2 * sizeof(game::EnemyView) +
    game::cfg::MAP_MAX_W * game::cfg::MAP_MAX_H * sizeof(game::TileView);

static constexpr size_t MAX_COMMAND_BYTES =
    game::cfg::UNIT_HARD_CAP * sizeof(game::Command);

// ---- Free functions ---------------------------------------------------------

std::vector<uint8_t> wat_to_wasm(std::string_view wat) {
  wasm_byte_vec_t result;
  wasmtime_error_t* err = wasmtime_wat2wasm(wat.data(), wat.size(), &result);
  if (err) {
    std::string msg = drain_error(err, nullptr);
    throw std::runtime_error("wat2wasm failed: " + msg);
  }
  std::vector<uint8_t> out(result.data, result.data + result.size);
  wasm_byte_vec_delete(&result);
  return out;
}

std::vector<uint8_t> load_wasm_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    throw std::runtime_error("cannot open wasm file: " + path);
  return {std::istreambuf_iterator<char>(f), {}};
}

// ---- WasmBot::Impl ----------------------------------------------------------

struct WasmBot::Impl {
  wasm_engine_t*     engine{nullptr};
  wasmtime_store_t*  store{nullptr};
  wasmtime_linker_t* linker{nullptr};
  wasmtime_module_t* module{nullptr};
  wasmtime_instance_t instance{};
  wasmtime_memory_t   memory{};
  wasmtime_func_t     fn_init{};
  wasmtime_func_t     fn_on_tick{};
  uint32_t snapshot_addr{0};
  uint32_t command_addr{0};

  wasmtime_context_t* ctx() { return wasmtime_store_context(store); }

  ~Impl() {
    if (module) wasmtime_module_delete(module);
    if (linker) wasmtime_linker_delete(linker);
    if (store)  wasmtime_store_delete(store);
    if (engine) wasm_engine_delete(engine);
  }
};

// ---- WasmBot ----------------------------------------------------------------

WasmBot::WasmBot(const std::vector<uint8_t>& wasm)
    : impl_(std::make_unique<Impl>()) {
  // Engine with determinism flags and fuel metering.
  wasm_config_t* cfg = wasm_config_new();
  wasmtime_config_consume_fuel_set(cfg, true);
  wasmtime_config_wasm_threads_set(cfg, false);
  wasmtime_config_wasm_simd_set(cfg, false);
  wasmtime_config_wasm_relaxed_simd_set(cfg, false); // relaxed SIMD requires SIMD
  wasmtime_config_cranelift_nan_canonicalization_set(cfg, true);
  impl_->engine = wasm_engine_new_with_config(cfg); // config ownership transferred

  impl_->store  = wasmtime_store_new(impl_->engine, nullptr, nullptr);
  impl_->linker = wasmtime_linker_new(impl_->engine);

  // Compile.
  wasmtime_error_t* err = wasmtime_module_new(
      impl_->engine, wasm.data(), wasm.size(), &impl_->module);
  if (err) { last_error_ = drain_error(err, nullptr); return; }

  // Instantiate (no host imports required for v1 bots).
  wasm_trap_t* trap = nullptr;
  err = wasmtime_linker_instantiate(
      impl_->linker, impl_->ctx(), impl_->module, &impl_->instance, &trap);
  if (err || trap) { last_error_ = drain_error(err, trap); return; }

  // Extract and validate required exports.
  auto* ctx = impl_->ctx();
  auto& inst = impl_->instance;
  wasmtime_extern_t item;

  auto get_export = [&](const char* name, uint8_t expected_kind) -> bool {
    if (!wasmtime_instance_export_get(ctx, &inst, name, strlen(name), &item)
        || item.kind != expected_kind) {
      last_error_ = std::string("missing or wrong-type export: ") + name;
      return false;
    }
    return true;
  };

  if (!get_export("memory",   WASMTIME_EXTERN_MEMORY)) return;
  impl_->memory = item.of.memory;

  if (!get_export("init",     WASMTIME_EXTERN_FUNC)) return;
  impl_->fn_init = item.of.func;

  if (!get_export("on_tick",  WASMTIME_EXTERN_FUNC)) return;
  impl_->fn_on_tick = item.of.func;

  if (!get_export("SNAPSHOT_ADDR", WASMTIME_EXTERN_GLOBAL)) return;
  {
    wasmtime_val_t val;
    wasmtime_global_get(ctx, &item.of.global, &val);
    if (val.kind != WASMTIME_I32) { last_error_ = "SNAPSHOT_ADDR not i32"; return; }
    impl_->snapshot_addr = static_cast<uint32_t>(val.of.i32);
  }

  if (!get_export("COMMAND_ADDR", WASMTIME_EXTERN_GLOBAL)) return;
  {
    wasmtime_val_t val;
    wasmtime_global_get(ctx, &item.of.global, &val);
    if (val.kind != WASMTIME_I32) { last_error_ = "COMMAND_ADDR not i32"; return; }
    impl_->command_addr = static_cast<uint32_t>(val.of.i32);
  }

  // Validate buffer regions fit in the bot's linear memory.
  size_t mem_bytes = wasmtime_memory_data_size(ctx, &impl_->memory);
  if (impl_->snapshot_addr + MAX_SNAPSHOT_BYTES > mem_bytes) {
    last_error_ = "SNAPSHOT_ADDR buffer too small for worst-case snapshot";
    return;
  }
  if (impl_->command_addr + MAX_COMMAND_BYTES > mem_bytes) {
    last_error_ = "COMMAND_ADDR buffer too small for max commands";
    return;
  }

  healthy_ = true;
}

WasmBot::~WasmBot() = default;

void WasmBot::on_init(const game::Map& /*map*/, uint32_t faction_id) {
  if (!healthy_) return;

  auto* ctx = impl_->ctx();
  wasmtime_context_set_fuel(ctx, game::cfg::FUEL_STARTUP);

  wasmtime_val_t arg{};
  arg.kind   = WASMTIME_I32;
  arg.of.i32 = static_cast<int32_t>(faction_id);

  wasm_trap_t* trap = nullptr;
  wasmtime_error_t* err =
      wasmtime_func_call(ctx, &impl_->fn_init, &arg, 1, nullptr, 0, &trap);
  if (err || trap) {
    last_error_ = drain_error(err, trap);
    healthy_ = false; // init failure is permanent
  }
}

std::vector<game::Command> WasmBot::on_tick(const game::Snapshot& snap) {
  if (!healthy_) return {};

  auto* ctx = impl_->ctx();
  uint8_t* mem = wasmtime_memory_data(ctx, &impl_->memory);

  // Serialize snapshot into WASM linear memory.
  uint32_t my_units_off = static_cast<uint32_t>(sizeof(game::SnapshotHeader));
  uint32_t enemies_off  = my_units_off +
      static_cast<uint32_t>(snap.my_units.size()) * sizeof(game::UnitView);
  uint32_t tiles_off    = enemies_off +
      static_cast<uint32_t>(snap.visible_enemies.size()) * sizeof(game::EnemyView);

  game::SnapshotHeader hdr = snap.header;
  hdr.my_units_off = my_units_off;
  hdr.enemies_off  = enemies_off;
  hdr.tiles_off    = tiles_off;

  uint8_t* dest = mem + impl_->snapshot_addr;
  std::memcpy(dest,              &hdr,                      sizeof(hdr));
  std::memcpy(dest + my_units_off, snap.my_units.data(),    snap.my_units.size() * sizeof(game::UnitView));
  std::memcpy(dest + enemies_off,  snap.visible_enemies.data(), snap.visible_enemies.size() * sizeof(game::EnemyView));
  std::memcpy(dest + tiles_off,    snap.visible_tiles.data(),   snap.visible_tiles.size() * sizeof(game::TileView));

  // Refuel and call on_tick.
  wasmtime_context_set_fuel(ctx, game::cfg::FUEL_PER_TICK);

  wasmtime_val_t result{};
  wasm_trap_t* trap = nullptr;
  wasmtime_error_t* err =
      wasmtime_func_call(ctx, &impl_->fn_on_tick, nullptr, 0, &result, 1, &trap);
  if (err || trap) {
    // Tick forfeit — log but stay healthy for next tick.
    last_error_ = drain_error(err, trap);
    return {};
  }

  auto cmd_count = static_cast<uint32_t>(result.of.i32);
  cmd_count = std::min(cmd_count, static_cast<uint32_t>(game::cfg::UNIT_HARD_CAP));

  std::vector<game::Command> cmds(cmd_count);
  std::memcpy(cmds.data(), mem + impl_->command_addr,
              cmd_count * sizeof(game::Command));
  return cmds;
}

} // namespace runner
