#include "hikv.h"

#include <libpmem.h>

#include <iostream>

#define PERM_rw_r__r__ 0644
#define PERM_rwxr_xr_x 0755

#define PRINT_FILE_LINE() \
  std::cout << "file: " << __FILE__ << ": " << __LINE__ << std::endl

namespace hikv {

template <typename E>
static void PrintErrorThenExit(E&& err) {
  std::cerr << std::forward<E>(err) << std::endl;
  exit(1);
}

SimpleLogger::SimpleLogger(void* addr, size_t len) : addr_(addr), len_(len) {}

SimpleLogger::~SimpleLogger() { pmem_unmap(addr_, len_); }

void* OpenPMemFileThenInit(const std::string& path, size_t len) {
  size_t mapped_len;
  int is_pmem;
  void* addr = pmem_map_file(path.c_str(), len, PMEM_FILE_CREATE,
                             PERM_rw_r__r__, &mapped_len, &is_pmem);
  if (addr == nullptr) {
    PRINT_FILE_LINE();
    PrintErrorThenExit(pmem_errormsg());
  }
  return addr;
}

}  // namespace hikv
