#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "config.h"
#include "msg_queue.h"

namespace open_hikv::pmem {

class MessageQueueUnit {
 public:
  ErrorCode Push(MessageType type, const Slice& first, uint64_t second);

  ErrorCode Peek(MessageType* type, Slice* first, uint64_t* second,
                 int worker_idx);

  ErrorCode Pop(bool* more);

  ErrorCode WaitDrain(uint64_t version);

  void WaitUntilNotEmpty();

  uint64_t GetCurrentVersion() { return ver_; }

 private:
  void MayConsumeNotifyMessages();

  std::mutex mtx_;
  std::condition_variable cv_;
  std::atomic<uint64_t> ver_{0};
  std::atomic<uint64_t> applied_ver_{0};
  std::deque<std::tuple<MessageType, std::string, uint64_t>> que_;
  bool waiting_ = false;
};

class MessageQueueImpl : public MessageQueue {
 public:
  MessageQueueImpl();

  ~MessageQueueImpl();

  ErrorCode Push(MessageType type, const Slice& first,
                 uint64_t second) override;

  ErrorCode Peek(MessageType* type, Slice* first, uint64_t* second,
                 int worker_idx) const override;

  ErrorCode Pop(int worker_idx) override;

  ErrorCode WaitDrain() override;

  ErrorCode RegisterConsumeFunction(
      std::function<ErrorCode(int)> func) override;

 private:
  mutable std::vector<MessageQueueUnit> que_vec_;
  std::vector<std::unique_ptr<std::thread>> workers_;
};

}  // namespace open_hikv::pmem