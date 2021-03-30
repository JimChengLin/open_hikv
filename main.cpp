#include <iostream>

namespace open_hikv {
void RunOpenHiKVTest();
}
void RunBPlusTreeTest();

int main() {
  open_hikv::RunOpenHiKVTest();
  RunBPlusTreeTest();
  std::cout << "Done." << std::endl;
  return 0;
}
