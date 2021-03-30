#pragma once

#include "error_code.h"
#include "slice.h"

namespace open_hikv {

class Store {
 public:
  virtual ~Store() = default;

  virtual ErrorCode Get(uint64_t offset, Slice* k, Slice* v) const = 0;

  virtual ErrorCode Set(const Slice& k, const Slice& v, uint64_t* offset) = 0;

  virtual ErrorCode Del(uint64_t offset) = 0;
};

}  // namespace open_hikv