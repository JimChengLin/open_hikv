#pragma once

#include <atomic>
#include <mutex>
#include <vector>

#include "store.h"
#include "unordered_index.h"

namespace open_hikv::pmem {

class UnorderedIndexImpl : public UnorderedIndex {
 public:
  UnorderedIndexImpl(Store* store, const std::string& path, size_t shard_size,
                     size_t shard_num);

  ~UnorderedIndexImpl() override = default;

  ErrorCode Get(const Slice& k, Slice* v) const override;

  ErrorCode Set(const Slice& k, const Slice& v, uint64_t* offset) override;

  ErrorCode Del(const Slice& k) override { return ErrorCode::kOk; }

 private:
  Store* store_;
  const std::string path_;
  const size_t kShardSize;
  const size_t kEntryNum;

  struct Shard {
    char* data;
    std::atomic<size_t> max_probe_len;
  };

  std::vector<Shard> shards_;
};

}  // namespace open_hikv