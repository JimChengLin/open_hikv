#include "pmem_ordered_index_impl.h"

namespace open_hikv::pmem {

OrderedIndexImpl::OrderedIndexImpl(Store* store) : store_(store) {
  btree_ = bplus_tree_init(16, 16);
}

OrderedIndexImpl::~OrderedIndexImpl() { bplus_tree_deinit(btree_); }

// it leaks!
ErrorCode OrderedIndexImpl::Set(const Slice& k, uint64_t offset) {
  char* ptr = (char*)malloc(4 + k.size());
  int len = k.size();
  memcpy(ptr, &len, 4);
  memcpy(ptr + 4, k.data(), k.size());
  bplus_tree_put(btree_, reinterpret_cast<uintptr_t>(ptr), offset, 1);
  return ErrorCode::kOk;
}

ErrorCode OrderedIndexImpl::Del(const Slice& k) { return ErrorCode::kOk; }

ErrorCode OrderedIndexImpl::Scan(
    const Slice& k,
    const std::function<bool(const Slice&, const Slice&)>& func) const {
  return ErrorCode::kOk;
}

}  // namespace open_hikv::pmem
