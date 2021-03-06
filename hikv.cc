#include "hikv.h"

#include <libpmem.h>

#include <cassert>
#include <cstring>
#include <iostream>

#define PERM_rw_r__r__ 0644
#define PERM_rwxr_xr_x 0755

#define PRINT_FILE_LINE() \
  std::cout << "pos: " << __FILE__ << ":" << __LINE__ << std::endl

namespace hikv {

template <typename E>
static void PrintErrorThenExit(E&& err) {
  std::cerr << std::forward<E>(err) << std::endl;
  exit(1);
}

SimpleLogger::SimpleLogger(void* addr, size_t len) : addr_(addr), len_(len) {}

SimpleLogger::~SimpleLogger() { pmem_unmap(addr_, len_); }

// 32bit key length + 32bit value length + key content + value content
// I believe the layout matches what the paper said
uint64_t SimpleLogger::Add(const std::string_view& k,
                           const std::string_view& v) {
  uint32_t k_len;
  uint32_t v_len;
  uint64_t request_len = sizeof(k_len) + sizeof(v_len) + k.size() + v.size();

  uint64_t offset = curr_offset_.fetch_add(request_len);
  if (offset + request_len > len_) {
    PRINT_FILE_LINE();
    PrintErrorThenExit("out of space. offset: " + std::to_string(offset));
  }
  assert(offset < (1ULL << 48));

  k_len = k.size();
  v_len = v.size();

  char* cursor = reinterpret_cast<char*>(addr_) + offset;
  memcpy(cursor, &k_len, sizeof(k_len));
  cursor += sizeof(k_len);
  memcpy(cursor, &v_len, sizeof(v_len));
  cursor += sizeof(v_len);
  memcpy(cursor, k.data(), k.size());
  cursor += k.size();
  memcpy(cursor, v.data(), v.size());
  pmem_persist(cursor, request_len);
  return offset;
}

bool SimpleLogger::Get(uint64_t offset, const std::string_view& k,
                       std::string* v) const {
  uint32_t k_len;
  uint32_t v_len;

  char* cursor = reinterpret_cast<char*>(addr_) + offset;
  memcpy(&k_len, cursor, sizeof(k_len));
  cursor += sizeof(k_len);
  memcpy(&v_len, cursor, sizeof(v_len));
  cursor += sizeof(v_len);

  std::string_view store_k(cursor, k_len);
  if (store_k == k) {
    if (v != nullptr) {
      cursor += k_len;
      v->assign(cursor, v_len);
    }
    return true;
  }
  return false;
}

// The hash map uses two buckets according to the graph in the paper
SimpleLockFreeHash::SimpleLockFreeHash(void* addr, size_t len,
                                       SimpleLogger* logger)
    : addr_(addr),
      len_(len),
      slot_num_(len / sizeof(HashEntry) / 2),
      logger_(logger) {
  assert(len % sizeof(HashEntry) == 0);
  assert(len / sizeof(HashEntry) % 2 == 0);
}

SimpleLockFreeHash::~SimpleLockFreeHash() { pmem_unmap(addr_, len_); }

ErrorCode SimpleLockFreeHash::Add(const std::string_view& k,
                                  const std::string_view& v) {
  // may leak
  uint64_t offset = logger_->Add(k, v);

  uint64_t hash = std::hash<std::string_view>()(k) | 1;
  auto [bucket_0, bucket_1] = GetTwoBuckets(hash);

  auto try_install = [&](HashEntry* entry) -> bool {
    if (entry->signature != 0) {
      return false;
    }

    __int128_t old_val{0};
    HashEntry new_entry;
    new_entry.signature = hash;
    new_entry.offset = offset;
    __int128_t new_val;
    memcpy(&new_val, &new_entry, sizeof(new_entry));

    bool success = __sync_bool_compare_and_swap(
        reinterpret_cast<__int128_t*>(entry), old_val, new_val);
    if (success) {
      pmem_persist(entry, sizeof(new_entry));
    }
    return success;
  };

  if (try_install(bucket_0) || try_install(bucket_1)) {
    return ErrorCode::kOK;
  }
  return ErrorCode::kHashCollision;
}

ErrorCode SimpleLockFreeHash::Get(const std::string_view& k,
                                  std::string* v) const {
  return {};
}

std::pair<SimpleLockFreeHash::HashEntry*, SimpleLockFreeHash::HashEntry*>
SimpleLockFreeHash::GetTwoBuckets(uint64_t hash) {
  uint64_t slot_i = hash % slot_num_;
  uint64_t bucket_i = slot_i * 2;
  HashEntry* bucket_0 = reinterpret_cast<HashEntry*>(
      reinterpret_cast<char*>(addr_) + bucket_i * sizeof(HashEntry));
  return {bucket_0, bucket_0 + 1};
}

void* OpenPMemFileThenInit(const std::string& path, size_t len) {
  size_t mapped_len;
  int is_pmem;
  void* addr = pmem_map_file(path.c_str(), len, PMEM_FILE_CREATE,
                             PERM_rw_r__r__, &mapped_len, &is_pmem);
  if (addr == nullptr) {
    PRINT_FILE_LINE();
    PrintErrorThenExit(pmem_errormsg());
  }
  // init file
  pmem_memset_persist(addr, 0, len);
  return addr;
}

}  // namespace hikv
