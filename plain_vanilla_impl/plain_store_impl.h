#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

#include "store.h"

namespace open_hikv::plain_vanilla {

class StoreImpl : public Store {
 public:
  ~StoreImpl() override = default;

  ErrorCode Get(uint64_t offset, Slice* k, Slice* v) const override;

  ErrorCode Set(const Slice& k, const Slice& v, uint64_t* offset) override;

  ErrorCode Del(uint64_t offset) override;

 private:
  mutable std::mutex mtx_;
  std::unordered_map<uint64_t, std::pair<std::string, std::string>>
      off2str_map_;
  uint64_t offset_ = 0;
};

}  // namespace open_hikv::plain_vanilla