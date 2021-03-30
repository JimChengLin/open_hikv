#include "plain_unordered_index_impl.h"

namespace open_hikv::plain_vanilla {

ErrorCode UnorderedIndexImpl::Get(const Slice& k, Slice* v) const {
  std::lock_guard<std::mutex> guard(mtx_);
  auto it = key2off_map_.find(k.ToString());
  if (it != key2off_map_.end()) {
    return store_->Get(it->second, nullptr, v);
  }
  return ErrorCode::kNotFound;
}

ErrorCode UnorderedIndexImpl::Set(const Slice& k, const Slice& v,
                                  uint64_t* offset) {
  std::lock_guard<std::mutex> guard(mtx_);
  ErrorCode code = store_->Set(k, v, offset);
  if (code != ErrorCode::kOk) {
    return code;
  }
  key2off_map_[k.ToString()] = *offset;
  return ErrorCode::kOk;
}

ErrorCode UnorderedIndexImpl::Del(const Slice& k) {
  std::lock_guard<std::mutex> guard(mtx_);
  key2off_map_.erase(k.ToString());
  return ErrorCode::kOk;
}

}  // namespace open_hikv::plain_vanilla