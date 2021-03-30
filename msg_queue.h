#pragma once

#include <functional>

#include "error_code.h"
#include "slice.h"

namespace open_hikv {

enum class MessageType {
  kSet,
  kDel,
  kNotify,  // for internal usage
  kClose,   // for internal usage
};

class MessageQueue {
 public:
  virtual ~MessageQueue() = default;

  virtual ErrorCode Push(MessageType type, const Slice& first,
                         uint64_t second) = 0;

  virtual ErrorCode Peek(MessageType* type, Slice* first, uint64_t* second,
                         int worker_idx) const = 0;

  virtual ErrorCode Pop(int worker_idx) = 0;

  virtual ErrorCode WaitDrain() = 0;

  virtual ErrorCode RegisterConsumeFunction(
      std::function<ErrorCode(int)> func) = 0;
};

}  // namespace open_hikv