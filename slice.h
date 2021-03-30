#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>
#include <type_traits>

namespace open_hikv {

class Slice {
 private:
  const char* data_ = "";
  size_t size_ = 0;

 public:
  Slice() = default;

  Slice(const char* d, size_t n) : data_(d), size_(n) {}

  template <typename T>
  Slice(const T* s, size_t n) : Slice(reinterpret_cast<const char*>(s), n) {
    static_assert(sizeof(T) == sizeof(char), "");
  }

  Slice(const std::string& s) : Slice(s.data(), s.size()) {}

  template <typename T, typename = std::enable_if_t<
                            std::is_convertible<T, const char*>::value>>
  Slice(T s) : data_(s), size_(strlen(s)) {}

  template <size_t L>
  Slice(const char (&s)[L]) : data_(s), size_(L - 1) {}

 public:
  // same as STL
  const char* data() const { return data_; }

  // same as STL
  size_t size() const { return size_; }

  const char& operator[](size_t n) const {
    assert(n < size_);
    return data_[n];
  }

  bool operator==(const Slice& another) const {
    return size_ == another.size_ && memcmp(data_, another.data_, size_) == 0;
  }

  bool operator!=(const Slice& another) const { return !operator==(another); }

  std::string ToString() const { return {data_, size_}; }
};

}  // namespace open_hikv