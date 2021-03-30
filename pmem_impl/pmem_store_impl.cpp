#include "pmem_store_impl.h"

#include "pmem_op.h"

#define PERM_rw_r__r__ 0644
#define PERM_rwxr_xr_x 0755

namespace open_hikv::pmem {
using namespace terark;

struct Header {
  uint32_t k_len;
  uint32_t v_len;
};

ErrorCode StoreImpl::Get(uint64_t offset, Slice* k, Slice* v) const {
  char* ptr = reinterpret_cast<char*>(static_cast<uintptr_t>(offset));
  Header header;
  memcpy(&header, ptr, sizeof(header));
  ptr += sizeof(header);
  if (k != nullptr) {
    *k = Slice(ptr, header.k_len);
  }
  if (v != nullptr) {
    *v = Slice(ptr + header.k_len, header.v_len);
  }
  return ErrorCode::kOk;
}

ErrorCode StoreImpl::Set(const Slice& k, const Slice& v, uint64_t* offset) {
  uint64_t request_len = sizeof(Header) + k.size() + v.size();
  auto& table = GetTLSTable();
  if (table.offset + request_len > kTableSize) {
    AllocateNewTable(table);
  }

  char* ptr = table.data + table.offset;
  *offset = reinterpret_cast<uintptr_t>(ptr);
  table.offset += request_len;

  Header header;
  header.k_len = k.size();
  header.v_len = v.size();
  memcpy(ptr, &header, sizeof(header));
  ptr += sizeof(header);
  memcpy(ptr, k.data(), k.size());
  ptr += k.size();
  memcpy(ptr, v.data(), v.size());
  PMemPersistRange(reinterpret_cast<void*>(static_cast<uintptr_t>(*offset)),
                   request_len);
  return ErrorCode::kOk;
}

StoreImpl::Table& StoreImpl::GetTLSTable() {
  static thread_local Table tls_table;
  return tls_table;
}

// WARNING: it leaks! but fine for benchmark
void StoreImpl::AllocateNewTable(Table& table) {
  const auto& filename =
      path_ + "/" + std::to_string(table_id_++) + ".open_hikv_store_pool";
  size_t mapped_len;
  int is_pmem;
  auto addr = reinterpret_cast<char*>(
      pmem_map_file(filename.c_str(), kTableSize, PMEM_FILE_CREATE,
                    PERM_rw_r__r__, &mapped_len, &is_pmem));
  table.data = addr;
  table.offset = 0;
}

}  // namespace open_hikv::pmem