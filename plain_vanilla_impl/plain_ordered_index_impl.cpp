#include "plain_ordered_index_impl.h"

namespace open_hikv::plain_vanilla {

ErrorCode OrderedIndexImpl::Set(const Slice& k, uint64_t offset) {
  std::lock_guard<std::mutex> guard(mtx_);
  key2off_map_[k.ToString()] = offset;
  return ErrorCode::kOk;
}

ErrorCode OrderedIndexImpl::Del(const Slice& k) {
  std::lock_guard<std::mutex> guard(mtx_);
  auto it = key2off_map_.find(k.ToString());
  if (it != key2off_map_.end()) {
    ErrorCode code = store_->Del(it->second);
    if (code != ErrorCode::kOk) {
      return code;
    }
    key2off_map_.erase(it);
  }
  return ErrorCode::kOk;
}

ErrorCode OrderedIndexImpl::Scan(
    const Slice& k,
    const std::function<bool(const Slice&, const Slice&)>& func) const {
  std::lock_guard<std::mutex> guard(mtx_);
  auto it = key2off_map_.lower_bound(k.ToString());
  if (it != key2off_map_.end()) {
    do {
      Slice val;
      ErrorCode code = store_->Get(it->second, nullptr, &val);
      if (code != ErrorCode::kOk) {
        return code;
      }

      bool ok = func(it->first, val);
      if (!ok) {
        break;
      }
      ++it;
    } while (it != key2off_map_.end());
    return ErrorCode::kOk;
  }
  return ErrorCode::kNotFound;
}

}  // namespace open_hikv::plain_vanilla