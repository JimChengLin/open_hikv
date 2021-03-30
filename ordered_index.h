#pragma once

#include <functional>

#include "error_code.h"
#include "slice.h"

namespace open_hikv {

class OrderedIndex {
 public:
  virtual ~OrderedIndex() = default;

  virtual ErrorCode Set(const Slice& k, uint64_t offset) = 0;

  virtual ErrorCode Del(const Slice& k) = 0;

  virtual ErrorCode Scan(
      const Slice& k,
      const std::function<bool(const Slice&, const Slice&)>& func) const = 0;
};

}  // namespace open_hikv