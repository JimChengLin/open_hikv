#include "plain_msg_queue_impl.h"

#include <chrono>
#include <iostream>
#include <utility>

namespace open_hikv::plain_vanilla {

MessageQueueImpl::~MessageQueueImpl() {
  if (worker_) {
    stop_ = true;
    worker_->join();
  }
}

ErrorCode MessageQueueImpl::Push(MessageType type, const Slice& first,
                                 uint64_t second) {
  std::lock_guard<std::mutex> guard(mtx_);
  que_.emplace_back(type, first.ToString(), second);
  ++ver_;
  return ErrorCode::kOk;
}

ErrorCode MessageQueueImpl::Peek(MessageType* type, Slice* first,
                                 uint64_t* second, int worker_idx) const {
  std::lock_guard<std::mutex> guard(mtx_);
  if (que_.empty()) {
    return ErrorCode::kNotFound;
  }
  const auto& msg = que_.front();
  std::tie(*type, *first, *second) = msg;
  return ErrorCode::kOk;
}

ErrorCode MessageQueueImpl::Pop(int worker_idx) {
  std::lock_guard<std::mutex> guard(mtx_);
  if (que_.empty()) {
    return ErrorCode::kNotFound;
  }
  que_.pop_front();
  ++applied_ver_;
  return ErrorCode::kOk;
}

ErrorCode MessageQueueImpl::WaitDrain() {
  std::unique_lock<std::mutex> lock(mtx_);
  uint64_t target_ver = ver_;
  while (target_ver > applied_ver_) {
    lock.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    lock.lock();
  }
  return ErrorCode::kOk;
}

ErrorCode MessageQueueImpl::RegisterConsumeFunction(
    std::function<ErrorCode(int)> func) {
  worker_ = std::make_unique<std::thread>([&, func]() {
    while (!stop_) {
      if (ver_ == applied_ver_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      ErrorCode code = func(0);
      if (code != ErrorCode::kOk) {
        std::cerr << "MessageQueue Consumer Down: " << static_cast<int>(code)
                  << std::endl;
        exit(1);
      }
    }
  });
  return ErrorCode::kOk;
}

}  // namespace open_hikv::plain_vanilla