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
    : addr_(addr), len_(len), logger_(logger) {
  assert(len % sizeof(HashEntry) == 0);
  assert(len / sizeof(HashEntry) % 2 == 0);
}

SimpleLockFreeHash::~SimpleLockFreeHash() { pmem_unmap(addr_, len_); }

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
