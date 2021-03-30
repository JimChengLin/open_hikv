#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <tuple>

#include "msg_queue.h"

namespace open_hikv::plain_vanilla {

class MessageQueueImpl : public MessageQueue {
 public:
  ~MessageQueueImpl() override;

  ErrorCode Push(MessageType type, const Slice& first,
                 uint64_t second) override;

  ErrorCode Peek(MessageType* type, Slice* first, uint64_t* second,
                 int worker_idx) const override;

  ErrorCode Pop(int worker_idx) override;

  ErrorCode WaitDrain() override;

  ErrorCode RegisterConsumeFunction(
      std::function<ErrorCode(int)> func) override;

 private:
  mutable std::mutex mtx_;
  uint64_t ver_ = 0;
  uint64_t applied_ver_ = 0;
  std::deque<std::tuple<MessageType, std::string, uint64_t>> que_;

  std::atomic<bool> stop_{false};
  std::unique_ptr<std::thread> worker_;
};

}  // namespace open_hikv::plain_vanilla