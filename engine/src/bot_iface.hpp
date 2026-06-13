#pragma once
#include "snapshot.hpp"
#include "command.hpp"
#include <vector>

namespace game {

struct Map;

// Abstract interface implemented by both in-process scripted bots (tests)
// and the Wasmtime runner (Phase 4). The engine only talks to this interface.
class Bot {
public:
  virtual ~Bot() = default;

  // Called once before tick 0. Fat budget; bot allocates/precomputes here.
  virtual void on_init(const Map& map, uint32_t faction_id) = 0;

  // Called every tick. Returns raw commands; engine validates them.
  // Implementations MUST NOT retain a reference to `snap` after returning.
  virtual std::vector<Command> on_tick(const Snapshot& snap) = 0;

  // Whether the bot is still functional (false after init failure or
  // repeated OOM/trap — WASM runner sets this; scripted bots always true).
  virtual bool healthy() const { return true; }
};

} // namespace game
