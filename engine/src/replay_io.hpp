#pragma once
#include "file_io.hpp"
#include "replay.hpp"
#include <stdexcept>
#include <string>

namespace game {

// Thrown when reading a replay whose ABI version doesn't match this engine build.
struct ReplayVersionError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Streaming replay writer — call begin(), write_tick() per tick, then finish().
// Backed by any FileWriter; use MemoryWriter in tests, ZstdFileWriter in production.
class ReplayWriter {
public:
  explicit ReplayWriter(FileWriter& out);
  void begin(const ReplayLog& header);
  void write_tick(const TickEntry& entry);
  void finish(uint64_t expected_hash, const MatchResult& outcome);

private:
  FileWriter& out_;
};

// Convenience: serialize a fully-populated in-memory ReplayLog in one shot.
void write_replay(const ReplayLog& log, FileWriter& out);
ReplayLog read_replay(FileReader& in);

// Convenience file I/O — deduces compression from extension (.sfbg = raw, .sfbg.zst = zstd).
void write_replay_file(const ReplayLog& log, const std::string& path);
ReplayLog read_replay_file(const std::string& path);

} // namespace game
