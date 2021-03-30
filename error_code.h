#pragma once

namespace open_hikv {

enum class ErrorCode {
  kOk = 0,
  kNotFound,
  kMessageQueueEmpty,
  kClose,
};

}