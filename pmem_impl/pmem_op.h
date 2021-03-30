#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "cpu.h"

#ifndef TERARK_LINK_LIBPMEM
#include <fcntl.h>
#include <immintrin.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xmmintrin.h>

#include <cerrno>

#include "unit.h"
#else
#include <libpmem.h>
#endif

#ifndef TERARK_LINK_LIBPMEM
#define PMEM_FILE_CREATE 0
#define PMEM_F_MEM_NONTEMPORAL 0

namespace terark {
void pmem_memcpy(void* dst, const void* src, size_t n, unsigned flags);

inline void* pmem_map_file(const char* path, size_t len, int flags, mode_t mode,
                           size_t* mapped_lenp, int* is_pmemp) {
  assert(flags == PMEM_FILE_CREATE);
  int fd = open(path, O_CREAT | O_RDWR, mode);
  if (fd < 0) {
    return nullptr;
  }
  ftruncate(fd, len);
  int r = fallocate(fd, 0, 0, static_cast<off_t>(len));
  if (r != 0) {
    close(fd);
    return nullptr;
  }
#define roundup(x, y) ((((x) + ((y)-1)) / (y)) * (y))
  constexpr unsigned int kHintRequestSize = 6_MB;
  constexpr unsigned int kAlignment = 2_MB;
  void* hint_addr = mmap(nullptr, kHintRequestSize, PROT_READ,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (hint_addr != MAP_FAILED) {
    munmap(hint_addr, kHintRequestSize);
    hint_addr = reinterpret_cast<void*>(
        roundup(reinterpret_cast<uintptr_t>(hint_addr), kAlignment));
  } else {
    hint_addr = nullptr;
  }
#undef roundup
  void* addr = mmap(hint_addr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  if (addr == MAP_FAILED) {
    return nullptr;
  }
  *mapped_lenp = len;
  *is_pmemp = true;
  return addr;
}

inline void pmem_unmap(void* addr, size_t len) { munmap(addr, len); }

inline const char* pmem_errormsg() { return strerror(errno); }
}  // namespace terark
#endif

namespace terark {
inline bool HasAVX512() {
  static bool has = is_cpu_avx512f_present();
  return has;
}

inline void PMemCopy(void* dst, const void* src, size_t n) {
  if (!HasAVX512()) {
    memcpy(dst, src, n);
    return;
  }
  constexpr unsigned int kGranularity = 256;
  if (n == 0) return;
  char* ptr = reinterpret_cast<char*>(dst);
  const char* begin = reinterpret_cast<const char*>(src);
  size_t remainder = reinterpret_cast<uintptr_t>(dst) % kGranularity;
  if (remainder != 0) {
    size_t unaligned_bytes = kGranularity - remainder;
    if (unaligned_bytes < n) {
      pmem_memcpy(ptr, begin, unaligned_bytes, PMEM_F_MEM_NONTEMPORAL);
      ptr += unaligned_bytes;
      begin += unaligned_bytes;
      n -= unaligned_bytes;
    }
    assert(reinterpret_cast<uintptr_t>(ptr) % kGranularity == 0 ||
           n < kGranularity);
  }
  const char* end = begin + n;
  for (; begin + kGranularity < end;
       ptr += kGranularity, begin += kGranularity) {
#if !defined(TERARK_LINK_LIBPMEM) && defined(__AVX512F__)
    __m512i zmm0 = _mm512_loadu_si512((const __m512i*)(begin) + 0);
    __m512i zmm1 = _mm512_loadu_si512((const __m512i*)(begin) + 1);
    __m512i zmm2 = _mm512_loadu_si512((const __m512i*)(begin) + 2);
    __m512i zmm3 = _mm512_loadu_si512((const __m512i*)(begin) + 3);
    _mm512_stream_si512((__m512i*)(ptr) + 0, zmm0);
    _mm512_stream_si512((__m512i*)(ptr) + 1, zmm1);
    _mm512_stream_si512((__m512i*)(ptr) + 2, zmm2);
    _mm512_stream_si512((__m512i*)(ptr) + 3, zmm3);
    _mm_sfence();
#else
    pmem_memcpy(ptr, begin, kGranularity, PMEM_F_MEM_NONTEMPORAL);
#endif
  }
  pmem_memcpy(ptr, begin, end - begin, PMEM_F_MEM_NONTEMPORAL);
#if !defined(TERARK_LINK_LIBPMEM) && defined(__AVX512F__)
  _mm256_zeroupper();
#endif
}

inline void PMemPersist(void* dst) {
  if (!HasAVX512()) {
    return;
  }
#ifndef TERARK_HAVE_MM_CLWB
  asm volatile(".byte 0x66; xsaveopt %0" : "+m"(*(volatile char*)(dst)));
#else
  _mm_clwb(dst);
#endif
  _mm_sfence();
}

inline void PMemPersistRange(void* addr, size_t len) {
  if (!HasAVX512()) {
    return;
  }
  uintptr_t uptr;

  /*
   * Loop through cache-line-size (typically 64B) aligned chunks
   * covering the given range.
   */
  for (uptr = (uintptr_t)addr & ~(64 - 1); uptr < (uintptr_t)addr + len;
       uptr += 64) {
    asm volatile(".byte 0x66; xsaveopt %0" : "+m"(*(volatile char*)(uptr)));
  }
  _mm_sfence();
}

template <typename T>
inline void PMemAtomicStore(void* dst, T val) {
  reinterpret_cast<std::atomic<T>*>(dst)->store(val, std::memory_order_relaxed);
  PMemPersist(dst);
}
}  // namespace terark
