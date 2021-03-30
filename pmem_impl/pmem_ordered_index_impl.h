#pragma once

#include "bplustree.h"
#include "ordered_index.h"
#include "store.h"

namespace open_hikv::pmem {

class OrderedIndexImpl : public OrderedIndex {
 public:
  explicit OrderedIndexImpl(Store* store);

  ~OrderedIndexImpl() override;

  ErrorCode Set(const Slice& k, uint64_t offset) override;

  ErrorCode Del(const Slice& k) override;

  ErrorCode Scan(const Slice& k,
                 const std::function<bool(const Slice&, const Slice&)>& func)
      const override;

 private:
  Store* store_;
  struct bplus_tree* btree_;
};

}  // namespace open_hikv::pmem
