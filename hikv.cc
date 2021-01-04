#include "hikv.h"

#include <libpmem.h>

#include <iostream>

namespace hikv {

template <typename E>
static void PrintErrorThenExit(E&& err) {
  std::cerr << std::forward<E>(err) << std::endl;
  exit(1);
}

SimpleLogger::SimpleLogger(void* addr, size_t len) : addr_(addr), len_(len) {}

SimpleLogger::~SimpleLogger() { pmem_unmap(addr_, len_); }

}  // namespace hikv
