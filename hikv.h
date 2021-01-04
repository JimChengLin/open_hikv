#pragma once

#include <atomic>
#include <cinttypes>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace hikv {

enum class ErrorCode : int {
  kOK,
  kOutOfSpace,
  kNotFound,
};

// SimpleLockFreeHash cannot grow dynamically
// Besides expansion, GC and recovery are not supported
// One logger serves one hash shard
class SimpleLogger {
 public:
  SimpleLogger(void* addr, size_t len);

  ~SimpleLogger();

 public:
  uint64_t Add(const std::string_view& k, const std::string_view& v);

  bool Get(const std::string_view& k, std::string* v) const;

 private:
  void* addr_;
  size_t len_;
  std::atomic<size_t> curr_offset_{0};
};

// SimpleLockFreeHash cannot grow dynamically
// "adds" may fail due to hash collisions
// I don't know how original HiKV to handle this
class SimpleLockFreeHash {
 public:
  SimpleLockFreeHash(void* addr, size_t len, SimpleLogger* logger);

  ~SimpleLockFreeHash();

  struct HashEntry {
    uint64_t signature;
    uint64_t offset;
  };

  ErrorCode Add(const std::string_view& k, const std::string_view& v);

  ErrorCode Get(const std::string_view& k, std::string* v) const;

 private:
  void* addr_;
  size_t len_;
  SimpleLogger* logger_;
};

class HiKV {
 public:
  HiKV(const std::string& path, size_t hash_shard_num = 257);

 public:
  ErrorCode Add(const std::string_view& k, const std::string_view& v);

  ErrorCode Get(const std::string_view& k, std::string* v) const;

  // TODO: def API
  ErrorCode Scan();

 private:
  std::vector<std::unique_ptr<SimpleLogger>> loggers_;
  std::vector<std::unique_ptr<SimpleLockFreeHash>> hash_maps_;
};

}  // namespace hikv
