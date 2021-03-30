#pragma once

#include <cassert>
#include <memory>

#include "msg_queue.h"
#include "ordered_index.h"
#include "store.h"
#include "unordered_index.h"

namespace open_hikv {

class OpenHiKV {
 public:
  OpenHiKV(std::unique_ptr<Store> store,
           std::unique_ptr<UnorderedIndex> unordered_index,
           std::unique_ptr<OrderedIndex> ordered_index,
           std::unique_ptr<MessageQueue> message_queue)
      : store_(std::move(store)),
        unordered_index_(std::move(unordered_index)),
        ordered_index_(std::move(ordered_index)),
        message_queue_(std::move(message_queue)) {
    ErrorCode code = message_queue_->RegisterConsumeFunction(
        [this](int worker_idx) { return ConsumeMessageQueue(worker_idx); });
    (void)code;
    assert(code == ErrorCode::kOk);
  }

  ErrorCode Get(const Slice& k, Slice* v) const;

  ErrorCode Set(const Slice& k, const Slice& v);

  ErrorCode Del(const Slice& k);

  ErrorCode Scan(
      const Slice& k,
      const std::function<bool(const Slice&, const Slice&)>& func) const;

  ErrorCode ConsumeMessageQueue(int worker_idx);

  static ErrorCode OpenPlainVanillaOpenHiKV(std::unique_ptr<OpenHiKV>* kv);

  static ErrorCode OpenPMemOpenHiKV(std::unique_ptr<OpenHiKV>* kv);

 private:
  std::unique_ptr<Store> store_;
  std::unique_ptr<UnorderedIndex> unordered_index_;
  std::unique_ptr<OrderedIndex> ordered_index_;
  std::unique_ptr<MessageQueue> message_queue_;
};

}  // namespace open_hikv