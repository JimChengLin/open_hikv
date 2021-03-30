#include "plain_store_impl.h"

namespace open_hikv::plain_vanilla {

ErrorCode StoreImpl::Get(uint64_t offset, Slice* k, Slice* v) const {
  std::lock_guard<std::mutex> guard(mtx_);
  auto it = off2str_map_.find(offset);
  if (it != off2str_map_.end()) {
    if (k != nullptr) {
      *k = it->second.first;
    }
    if (v != nullptr) {
      *v = it->second.second;
    }
    return ErrorCode::kOk;
  }
  return ErrorCode::kNotFound;
}

ErrorCode StoreImpl::Set(const Slice& k, const Slice& v, uint64_t* offset) {
  std::lock_guard<std::mutex> guard(mtx_);
  off2str_map_[offset_] = std::make_pair(k.ToString(), v.ToString());
  *offset = offset_;
  ++offset_;
  return ErrorCode::kOk;
}

ErrorCode StoreImpl::Del(uint64_t offset) {
  std::lock_guard<std::mutex> guard(mtx_);
  auto it = off2str_map_.find(offset);
  if (it != off2str_map_.end()) {
    off2str_map_.erase(it);
    return ErrorCode::kOk;
  }
  return ErrorCode::kNotFound;
}

}  // namespace open_hikv::plain_vanilla