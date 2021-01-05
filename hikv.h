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
  kHashCollision,
  kError,
};

// SimpleLockFreeHash cannot grow dynamically
// Besides expansion, GC and recovery are not supported
// One logger serves one hash shard
// All ops are lock-free
// Max size is 2^48 bytes(returned offset only uses 48bit)
class SimpleLogger {
 public:
  SimpleLogger(void* addr, size_t len);

  ~SimpleLogger();

 public:
  uint64_t Add(const std::string_view& k, const std::string_view& v);

  bool Get(uint64_t offset, const std::string_view& k, std::string* v) const;

  // TODO: for scan
  bool Get(uint64_t offset, std::string* k, std::string* v) const;

 private:
  void* addr_;
  size_t len_;
  std::atomic<size_t> curr_offset_{0};
};

// SimpleLockFreeHash cannot grow dynamically as well
// "adds" may fail due to hash collisions
// I don't know how original HiKV to handle this
// All ops are lock-free
class SimpleLockFreeHash {
 public:
  SimpleLockFreeHash(void* addr, size_t len, SimpleLogger* logger);

  ~SimpleLockFreeHash();

 public:
  ErrorCode Add(const std::string_view& k, const std::string_view& v);

  ErrorCode Get(const std::string_view& k, std::string* v) const;

 private:
  void* addr_;
  size_t len_;
  size_t slot_num_;
  SimpleLogger* logger_;

  struct HashEntry {
    uint64_t signature;
    uint64_t offset;
  };

  std::pair<HashEntry*, HashEntry*> GetTwoBuckets(uint64_t hash);
};

class HiKV {
 public:
  HiKV(const std::string& path, size_t hash_shard_num = 257);

 public:
  // cannot fail for simplicity
  void Init();

  ErrorCode Add(const std::string_view& k, const std::string_view& v);

  ErrorCode Get(const std::string_view& k, std::string* v) const;

  // TODO: def API
  ErrorCode Scan() const;

 private:
  std::vector<std::unique_ptr<SimpleLogger>> loggers_;
  std::vector<std::unique_ptr<SimpleLockFreeHash>> hash_maps_;
};

// cannot fail for simplicity
void* OpenPMemFileThenInit(const std::string& path, size_t len);

}  // namespace hikv
