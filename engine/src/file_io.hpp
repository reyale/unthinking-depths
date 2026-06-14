#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace game {

enum class FileIoType { Raw, Zstd };

class FileWriter {
public:
  virtual ~FileWriter() = default;
  virtual void write(const void* data, size_t size) = 0;
  virtual void close() = 0;

  template <typename T>
  void write_val(const T& v) {
    write(&v, sizeof(v));
  }
};

class FileReader {
public:
  virtual ~FileReader() = default;
  // Returns true if all `size` bytes were read successfully.
  virtual bool read(void* data, size_t size) = 0;
  virtual bool eof() const = 0;
  virtual void close() = 0;

  template <typename T>
  T read_val() {
    T v{};
    if (!read(&v, sizeof(v)))
      throw std::runtime_error("replay file truncated");
    return v;
  }
};

// Deduce FileIoType from file extension: .zst/.zstd → Zstd, .sfbg → Raw.
FileIoType deduce_file_io_type(const std::string& path);

std::unique_ptr<FileWriter> make_file_writer(const std::string& path,
                                             FileIoType type = FileIoType::Raw);
std::unique_ptr<FileReader> make_file_reader(const std::string& path,
                                             FileIoType type = FileIoType::Raw);

// ---- In-memory implementations (for unit tests) ----------------------------

class MemoryWriter : public FileWriter {
public:
  std::vector<uint8_t> data;

  void write(const void* d, size_t size) override {
    const auto* p = static_cast<const uint8_t*>(d);
    data.insert(data.end(), p, p + size);
  }
  void close() override {}
};

class MemoryReader : public FileReader {
public:
  explicit MemoryReader(const std::vector<uint8_t>& src) : src_(src) {}

  bool read(void* dst, size_t size) override {
    size_t avail = src_.size() - pos_;
    if (avail < size) {
      eof_ = true;
      return false;
    }
    std::memcpy(dst, src_.data() + pos_, size);
    pos_ += size;
    return true;
  }

  bool eof() const override { return eof_; }
  void close() override {}

private:
  const std::vector<uint8_t>& src_;
  size_t pos_{0};
  bool eof_{false};
};

} // namespace game
