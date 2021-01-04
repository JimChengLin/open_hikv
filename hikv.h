#include <cinttypes>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace hikv {
enum class ErrorCode : int {
  kOK,
  kOutOfSpace,
};

// GC and recovery is not supported
class SimpleLogger {
 public:
  SimpleLogger(const std::string& dir, size_t alloc_chunk_size);

  ~SimpleLogger();

 public:
  uint64_t Add(const std::string_view& k, const std::string_view& v);

  bool Get(const std::string_view& k, std::string* v) const;

 private:
  const std::string dir_;
  const size_t kAllocChunkSize;
  std::vector<void*> addrs_;
};

// SimpleLockFreeHash cannot grow dynamically
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

  ErrorCode Get(const std::string_view& k, std::string* v);

 private:
  void* addr_;
  size_t len_;
  SimpleLogger* logger_;
};

}  // namespace hikv
