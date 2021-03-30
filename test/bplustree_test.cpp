#include "pmem_impl/bplustree.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <random>

void RunBPlusTreeTest() {
  constexpr auto kTestTimes = 10000;

  std::default_random_engine engine(std::random_device{}());
  std::uniform_int_distribution<long long> dist;

  {
    struct bplus_tree* bpt = bplus_tree_init(16, 16);
    std::map<long long, long long> std_map;

    for (int i = 0; i < kTestTimes; ++i) {
      auto k = dist(engine);
      auto v = dist(engine);
      bplus_tree_put(bpt, k, v, 0);
      std_map[k] = v;
    }

    for (auto& kv : std_map) {
      (void)kv;
      assert(bplus_tree_get(bpt, kv.first, 0) == kv.second);
    }

    bplus_tree_deinit(bpt);
  }
  {
    struct bplus_tree* bpt = bplus_tree_init(16, 16);
    std::map<std::string, long long> std_map;

    // it leaks!
    auto make_str = [](const std::string& s) {
      void* m_str = malloc(4 + s.size());
      int len = s.size();
      memcpy(m_str, &len, 4);
      memcpy((char*)m_str + 4, s.data(), s.size());
      return m_str;
    };

    for (int i = 0; i < kTestTimes; ++i) {
      auto k = std::to_string(dist(engine));
      auto v = dist(engine);
      std_map[k] = v;
      bplus_tree_put(bpt, (uintptr_t)make_str(k), v, 1);
    }

    for (auto& kv : std_map) {
      (void)kv;
      assert(bplus_tree_get(bpt, (uintptr_t)make_str(kv.first), 1) ==
             kv.second);
    }

    bplus_tree_deinit(bpt);
    std::cout << "done use strcmp." << std::endl;
  }
  std::cout << "RunBPlusTreeTest." << std::endl;
}