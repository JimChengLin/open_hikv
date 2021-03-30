#pragma once

#include "error_code.h"
#include "slice.h"

namespace open_hikv {

class UnorderedIndex {
 public:
  virtual ~UnorderedIndex() = default;

  virtual ErrorCode Get(const Slice& k, Slice* v) const = 0;

  virtual ErrorCode Set(const Slice& k, const Slice& v, uint64_t* offset) = 0;

  virtual ErrorCode Del(const Slice& k) = 0;
};

}  // namespace open_hikv