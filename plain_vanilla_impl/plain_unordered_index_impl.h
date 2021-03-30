#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

#include "store.h"
#include "unordered_index.h"

namespace open_hikv::plain_vanilla {

class UnorderedIndexImpl : public UnorderedIndex {
 public:
  explicit UnorderedIndexImpl(Store* store) : store_(store) {}

  ~UnorderedIndexImpl() override = default;

  ErrorCode Get(const Slice& k, Slice* v) const override;

  ErrorCode Set(const Slice& k, const Slice& v, uint64_t* offset) override;

  ErrorCode Del(const Slice& k) override;

 private:
  Store* store_;

  mutable std::mutex mtx_;
  std::unordered_map<std::string, uint64_t> key2off_map_;
};

}  // namespace open_hikv::plain_vanilla