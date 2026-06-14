#include "replay_io.hpp"
#include "config.hpp"
#include "grid.hpp"
#include <array>
#include <string>

// Binary format (little-endian, all integers native-endian on x86-64):
//   8 bytes  magic "SFBGRPLY"
//   4 bytes  abi_version  (uint32_t)
//   8 bytes  seed         (uint64_t)
//   4 bytes  map_w        (int32_t)
//   4 bytes  map_h        (int32_t)
//   4 bytes  tile_count   (uint32_t)
//   per tile: 1 byte terrain (uint8_t) + 4 bytes resource_amount (int32_t)
//   4 bytes  entity_count (uint32_t)
//   per entity: 4 faction_id + 1 type + 1 is_structure + 4 x + 4 y
//   4 bytes  faction_a  (uint32_t)
//   4 bytes  faction_b  (uint32_t)
//   1 byte   name_a_len + name_a_len bytes (UTF-8, max cfg::MAX_PLAYER_NAME_LEN)
//   1 byte   name_b_len + name_b_len bytes
//   4 bytes  tick_cap   (uint32_t)
//   --- tick stream (tag-byte framing, enables streaming writes) ---
//   per tick: 0x01 (uint8_t) + 4 tick + 4 cmd_count_a + (cmd_count_a * sizeof(Command))
//                                      + 4 cmd_count_b + (cmd_count_b * sizeof(Command))
//   end:      0x00 (uint8_t) + 8 expected_hash + 4 outcome.winner.value + 1 outcome.reason

namespace game {

namespace {

constexpr std::array<uint8_t, 8> MAGIC    = {'S', 'F', 'B', 'G', 'R', 'P', 'L', 'Y'};
constexpr uint8_t TAG_TICK = 0x01;
constexpr uint8_t TAG_END  = 0x00;

void write_header(FileWriter& out, const ReplayLog& h) {
  out.write(MAGIC.data(), 8);
  out.write_val(h.abi_version);
  out.write_val(h.seed);
  out.write_val(h.map_w);
  out.write_val(h.map_h);

  out.write_val(static_cast<uint32_t>(h.map_tiles.size()));
  for (const auto& t : h.map_tiles) {
    out.write_val(static_cast<uint8_t>(t.terrain));
    out.write_val(t.resource_amount);
  }

  out.write_val(static_cast<uint32_t>(h.initial_entities.size()));
  for (const auto& e : h.initial_entities) {
    out.write_val(e.faction_id);
    out.write_val(e.type);
    out.write_val(static_cast<uint8_t>(e.is_structure ? 1 : 0));
    out.write_val(e.x);
    out.write_val(e.y);
  }

  out.write_val(h.faction_a);
  out.write_val(h.faction_b);
  out.write_val(static_cast<uint8_t>(h.name_a.size()));
  out.write(h.name_a.data(), h.name_a.size());
  out.write_val(static_cast<uint8_t>(h.name_b.size()));
  out.write(h.name_b.data(), h.name_b.size());
  out.write_val(h.tick_cap);
}

void write_tick_entry(FileWriter& out, const TickEntry& entry) {
  out.write_val(TAG_TICK);
  out.write_val(entry.tick);
  out.write_val(static_cast<uint32_t>(entry.raw_a.size()));
  for (const auto& cmd : entry.raw_a)
    out.write_val(cmd);
  out.write_val(static_cast<uint32_t>(entry.raw_b.size()));
  for (const auto& cmd : entry.raw_b)
    out.write_val(cmd);
}

void write_footer(FileWriter& out, uint64_t expected_hash, const MatchResult& outcome) {
  out.write_val(TAG_END);
  out.write_val(expected_hash);
  out.write_val(outcome.winner.value);
  out.write_val(static_cast<uint8_t>(outcome.reason));
  out.close();
}

} // namespace

// ---- ReplayWriter -----------------------------------------------------------

ReplayWriter::ReplayWriter(FileWriter& out) : out_(out) {}

void ReplayWriter::begin(const ReplayLog& header) {
  write_header(out_, header);
}

void ReplayWriter::write_tick(const TickEntry& entry) {
  write_tick_entry(out_, entry);
}

void ReplayWriter::finish(uint64_t expected_hash, const MatchResult& outcome) {
  write_footer(out_, expected_hash, outcome);
}

// ---- Batch API --------------------------------------------------------------

void write_replay(const ReplayLog& log, FileWriter& out) {
  ReplayWriter rw(out);
  rw.begin(log);
  for (const auto& entry : log.ticks)
    rw.write_tick(entry);
  rw.finish(log.expected_hash, log.outcome);
}

ReplayLog read_replay(FileReader& in) {
  std::array<uint8_t, 8> magic{};
  if (!in.read(magic.data(), 8) || magic != MAGIC)
    throw std::runtime_error("not a valid replay file (bad magic)");

  ReplayLog log;
  log.abi_version = in.read_val<uint32_t>();
  if (log.abi_version != cfg::ABI_VERSION)
    throw ReplayVersionError("ABI version mismatch: file=" + std::to_string(log.abi_version) +
                             " engine=" + std::to_string(cfg::ABI_VERSION));

  log.seed  = in.read_val<uint64_t>();
  log.map_w = in.read_val<int32_t>();
  log.map_h = in.read_val<int32_t>();

  auto tile_count = in.read_val<uint32_t>();
  log.map_tiles.reserve(tile_count);
  for (uint32_t i = 0; i < tile_count; ++i) {
    Tile t;
    t.terrain         = static_cast<Terrain>(in.read_val<uint8_t>());
    t.resource_amount = in.read_val<int32_t>();
    log.map_tiles.push_back(t);
  }

  auto entity_count = in.read_val<uint32_t>();
  log.initial_entities.reserve(entity_count);
  for (uint32_t i = 0; i < entity_count; ++i) {
    EntityInit e;
    e.faction_id   = in.read_val<uint32_t>();
    e.type         = in.read_val<uint8_t>();
    e.is_structure = (in.read_val<uint8_t>() != 0);
    e.x            = in.read_val<int32_t>();
    e.y            = in.read_val<int32_t>();
    log.initial_entities.push_back(e);
  }

  log.faction_a = in.read_val<uint32_t>();
  log.faction_b = in.read_val<uint32_t>();
  auto read_name = [&]() {
    auto len = in.read_val<uint8_t>();
    std::string name(len, '\0');
    if (len > 0 && !in.read(name.data(), len))
      throw std::runtime_error("replay file truncated (name)");
    return name;
  };
  log.name_a   = read_name();
  log.name_b   = read_name();
  log.tick_cap = in.read_val<uint32_t>();

  while (true) {
    auto tag = in.read_val<uint8_t>();
    if (tag == TAG_END) break;
    if (tag != TAG_TICK)
      throw std::runtime_error("replay file corrupt (bad tick tag)");
    TickEntry entry;
    entry.tick = in.read_val<uint32_t>();
    auto ca = in.read_val<uint32_t>();
    entry.raw_a.reserve(ca);
    for (uint32_t j = 0; j < ca; ++j)
      entry.raw_a.push_back(in.read_val<Command>());
    auto cb = in.read_val<uint32_t>();
    entry.raw_b.reserve(cb);
    for (uint32_t j = 0; j < cb; ++j)
      entry.raw_b.push_back(in.read_val<Command>());
    log.ticks.push_back(std::move(entry));
  }

  log.expected_hash        = in.read_val<uint64_t>();
  log.outcome.winner.value = in.read_val<uint32_t>();
  log.outcome.reason       = static_cast<WinReason>(in.read_val<uint8_t>());

  return log;
}

void write_replay_file(const ReplayLog& log, const std::string& path) {
  auto writer = make_file_writer(path, deduce_file_io_type(path));
  write_replay(log, *writer);
}

ReplayLog read_replay_file(const std::string& path) {
  auto reader = make_file_reader(path, deduce_file_io_type(path));
  return read_replay(*reader);
}

} // namespace game
