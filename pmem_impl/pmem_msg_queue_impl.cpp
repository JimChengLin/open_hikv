#include "pmem_msg_queue_impl.h"

#include <array>
#include <cassert>
#include <condition_variable>
#include <iostream>

#include "config.h"

namespace open_hikv::pmem {

ErrorCode MessageQueueUnit::Push(MessageType type, const Slice& first,
                                 uint64_t second) {
  std::lock_guard<std::mutex> guard(mtx_);
  que_.emplace_back(type, first.ToString(), second);
  ++ver_;
  if (waiting_) {
    cv_.notify_all();
  }
  return ErrorCode::kOk;
}

ErrorCode MessageQueueUnit::Peek(MessageType* type, Slice* first,
                                 uint64_t* second, int worker_idx) {
  std::lock_guard<std::mutex> guard(mtx_);
  MayConsumeNotifyMessages();
  if (que_.empty()) {
    return ErrorCode::kNotFound;
  }
  const auto& msg = que_.front();
  std::tie(*type, *first, *second) = msg;
  assert(*type != MessageType::kNotify);
  return ErrorCode::kOk;
}

ErrorCode MessageQueueUnit::Pop(bool* more) {
  std::lock_guard<std::mutex> guard(mtx_);
  if (que_.empty()) {
    return ErrorCode::kNotFound;
  }
  que_.pop_front();
  ++applied_ver_;
  MayConsumeNotifyMessages();
  *more = !que_.empty();
  return ErrorCode::kOk;
}

ErrorCode MessageQueueUnit::WaitDrain(uint64_t version) {
  if (applied_ver_ < version) {
    std::unique_lock<std::mutex> lock(mtx_);
    if (applied_ver_ < version) {
      que_.emplace_back(MessageType::kNotify, "" /* ignored */,
                        0 /* ignored */);
      cv_.wait(lock, [&]() { return applied_ver_ >= version; });
    }
  }
  return ErrorCode::kOk;
}

void MessageQueueUnit::WaitUntilNotEmpty() {
  std::unique_lock<std::mutex> lock(mtx_);
  if (!que_.empty()) {
    return;
  }
  waiting_ = true;
  cv_.wait(lock, [&]() { return !que_.empty(); });
}

void MessageQueueUnit::MayConsumeNotifyMessages() {
  bool should_notify = false;
  while (!que_.empty()) {
    auto& msg = que_.front();
    if (std::get<0>(msg) != MessageType::kNotify) {
      break;
    } else {
      should_notify = true;
      que_.pop_front();
    }
  }
  if (should_notify) {
    cv_.notify_all();
  }
}

MessageQueueImpl::MessageQueueImpl()
    : que_vec_(kMessageQueueShardNum), workers_(kMessageQueueShardNum) {}

MessageQueueImpl::~MessageQueueImpl() {
  for (auto& que : que_vec_) {
    que.Push(MessageType::kClose, "", 0);
  }
  for (auto& worker : workers_) {
    if (worker) {
      worker->join();
    }
  }
}

inline static uint32_t Hash(const char* data, size_t n, uint32_t seed) {
  // Similar to murmur hash
  const uint32_t m = 0xc6a4a793;
  const uint32_t r = 24;
  const char* limit = data + n;
  uint32_t h = seed ^ (n * m);

  // Pick up four bytes at a time
  while (data + 4 <= limit) {
    uint32_t w;
    memcpy(&w, data, sizeof(w));
    data += sizeof(w);
    h += w;
    h *= m;
    h ^= (h >> 16);
  }

  // Pick up remaining bytes
  switch (limit - data) {
    case 3:
      h += static_cast<uint8_t>(data[2]) << 16;
    case 2:
      h += static_cast<uint8_t>(data[1]) << 8;
    case 1:
      h += static_cast<uint8_t>(data[0]);
      h *= m;
      h ^= (h >> r);
      break;
  }
  return h;
}

ErrorCode MessageQueueImpl::Push(MessageType type, const Slice& first,
                                 uint64_t second) {
  return que_vec_[Hash(first.data(), first.size(), 0) % que_vec_.size()].Push(
      type, first, second);
}

ErrorCode MessageQueueImpl::Peek(MessageType* type, Slice* first,
                                 uint64_t* second, int worker_idx) const {
  return que_vec_[worker_idx].Peek(type, first, second, worker_idx);
}

ErrorCode MessageQueueImpl::Pop(int worker_idx) {
  bool more = true;
  ErrorCode code = que_vec_[worker_idx].Pop(&more);
  if (code == ErrorCode::kOk && !more) {
    code = ErrorCode::kMessageQueueEmpty;
  }
  return code;
}

ErrorCode MessageQueueImpl::WaitDrain() {
  size_t rand_num = 0;
  std::array<uint64_t, kMessageQueueShardNum> ver_arr{};
  for (size_t i = 0; i < kMessageQueueShardNum; ++i) {
    uint64_t ver = que_vec_[i].GetCurrentVersion();
    ver_arr[i] = ver;
    rand_num += ver;
  }
  size_t n = 0;
  while (n != kMessageQueueShardNum) {
    size_t idx = rand_num++ % kMessageQueueShardNum;
    que_vec_[idx].WaitDrain(ver_arr[idx]);
    ++n;
  }
  return ErrorCode::kOk;
}

ErrorCode MessageQueueImpl::RegisterConsumeFunction(
    std::function<ErrorCode(int)> func) {
  for (int i = 0; i < kMessageQueueShardNum; ++i) {
    workers_[i] = std::make_unique<std::thread>(
        [&, func](int nth) {
          auto& que = que_vec_[nth];
          que.WaitUntilNotEmpty();
          while (true) {
            ErrorCode code = func(nth);
            if (code == ErrorCode::kClose) {
              return;
            } else if (code == ErrorCode::kMessageQueueEmpty) {
              que.WaitUntilNotEmpty();
            } else if (code != ErrorCode::kOk) {
              std::cerr << "MessageQueue Consumer Down: "
                        << static_cast<int>(code) << std::endl;
              exit(1);
            }
          }
        },
        i);
  }
  return ErrorCode::kOk;
}

}  // namespace open_hikv::pmem