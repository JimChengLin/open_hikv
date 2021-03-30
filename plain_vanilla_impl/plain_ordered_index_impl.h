#pragma once

#include <map>
#include <mutex>
#include <string>

#include "ordered_index.h"
#include "store.h"

namespace open_hikv::plain_vanilla {

class OrderedIndexImpl : public OrderedIndex {
 public:
  explicit OrderedIndexImpl(Store* store) : store_(store) {}

  ~OrderedIndexImpl() override = default;

  ErrorCode Set(const Slice& k, uint64_t offset) override;

  ErrorCode Del(const Slice& k) override;

  ErrorCode Scan(const Slice& k,
                 const std::function<bool(const Slice&, const Slice&)>& func)
      const override;

 private:
  Store* store_;

  mutable std::mutex mtx_;
  std::map<std::string, uint64_t> key2off_map_;
};

}  // namespace open_hikv::plain_vanilla