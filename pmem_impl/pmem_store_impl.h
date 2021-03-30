#pragma once

#include <atomic>
#include <string>

#include "store.h"

namespace open_hikv::pmem {

class StoreImpl : public Store {
 public:
  explicit StoreImpl(const std::string& path) : path_(path) {}

  ~StoreImpl() override = default;

  ErrorCode Get(uint64_t offset, Slice* k, Slice* v) const override;

  ErrorCode Set(const Slice& k, const Slice& v, uint64_t* offset) override;

  ErrorCode Del(uint64_t offset) { return ErrorCode::kOk; }

 private:
  enum {
    kTableSize = (1 << 20) * 4,
  };

  struct Table {
    char* data = nullptr;
    uint64_t offset = kTableSize;
  };

  static Table& GetTLSTable();

  void AllocateNewTable(Table& table);

  std::atomic<uint64_t> table_id_{0};
  const std::string path_;
};

}  // namespace open_hikv::pmem