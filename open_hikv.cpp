#include "open_hikv.h"

#include "plain_vanilla_impl/plain_msg_queue_impl.h"
#include "plain_vanilla_impl/plain_ordered_index_impl.h"
#include "plain_vanilla_impl/plain_store_impl.h"
#include "plain_vanilla_impl/plain_unordered_index_impl.h"
#include "pmem_impl/pmem_msg_queue_impl.h"
#include "pmem_impl/pmem_ordered_index_impl.h"
#include "pmem_impl/pmem_store_impl.h"
#include "pmem_impl/pmem_unordered_index_impl.h"

namespace open_hikv {

ErrorCode OpenHiKV::Get(const Slice& k, Slice* v) const {
  return unordered_index_->Get(k, v);
}

ErrorCode OpenHiKV::Set(const Slice& k, const Slice& v) {
  uint64_t offset = 0;
  ErrorCode code = unordered_index_->Set(k, v, &offset);
  if (code != ErrorCode::kOk) {
    return code;
  }
  return message_queue_->Push(MessageType::kSet, k, offset);
}

ErrorCode OpenHiKV::Del(const Slice& k) {
  ErrorCode code = unordered_index_->Del(k);
  if (code != ErrorCode::kOk) {
    return code;
  }
  return message_queue_->Push(MessageType::kDel, k, {});
}

ErrorCode OpenHiKV::Scan(
    const Slice& k,
    const std::function<bool(const Slice&, const Slice&)>& func) const {
  ErrorCode code = message_queue_->WaitDrain();
  if (code != ErrorCode::kOk) {
    return code;
  }
  return ordered_index_->Scan(k, func);
}

ErrorCode OpenHiKV::ConsumeMessageQueue(int worker_idx) {
  MessageType type;
  Slice first;
  uint64_t second;

  ErrorCode code = message_queue_->Peek(&type, &first, &second, worker_idx);
  if (code != ErrorCode::kOk) {
    return code;
  }

  switch (type) {
    case MessageType::kSet:
      code = ordered_index_->Set(first, second);
      break;

    case MessageType::kDel:
      code = ordered_index_->Del(first);
      break;

    case MessageType::kClose:
      code = ErrorCode::kClose;
      break;

    default:
      assert(false);
      break;
  }

  if (code == ErrorCode::kOk) {
    code = message_queue_->Pop(worker_idx);
  }
  return code;
}

ErrorCode OpenHiKV::OpenPlainVanillaOpenHiKV(std::unique_ptr<OpenHiKV>* kv) {
  // auto store = std::make_unique<plain_vanilla::StoreImpl>();
  auto store = std::make_unique<pmem::StoreImpl>("/tmp/");
  // auto unordered_index =
  // std::make_unique<plain_vanilla::UnorderedIndexImpl>(store.get());
  auto unordered_index = std::make_unique<pmem::UnorderedIndexImpl>(
      store.get(), "/tmp/", 10000 * 16, 20);
  auto ordered_index =
      std::make_unique<plain_vanilla::OrderedIndexImpl>(store.get());
  // auto ordered_index = std::make_unique<pmem::OrderedIndexImpl>(store.get());
  // auto message_queue = std::make_unique<plain_vanilla::MessageQueueImpl>();
  auto message_queue = std::make_unique<pmem::MessageQueueImpl>();
  *kv = std::make_unique<OpenHiKV>(std::move(store), std::move(unordered_index),
                                   std::move(ordered_index),
                                   std::move(message_queue));
  return ErrorCode::kOk;
}

ErrorCode OpenHiKV::OpenPMemOpenHiKV(std::unique_ptr<OpenHiKV>* kv) {
  return ErrorCode::kOk;
}

}  // namespace open_hikv