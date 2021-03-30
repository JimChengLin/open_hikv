#include "open_hikv.h"

#include <iostream>
#include <map>
#include <random>
#include <thread>
#include <vector>

namespace open_hikv {

void RunOpenHiKVTest() {
  constexpr auto kTestTimes = 10000;
  constexpr auto kThreadNum = 10;

  std::unique_ptr<OpenHiKV> kv;
  OpenHiKV::OpenPlainVanillaOpenHiKV(&kv);

  std::vector<std::thread> jobs;
  jobs.reserve(kThreadNum);
  for (int i = 0; i < kThreadNum; ++i) {
    jobs.emplace_back([&, i]() {
      std::map<std::string, std::string> std_map;
      std::default_random_engine engine(std::random_device{}());

      int j = 0;
      auto gen_random_str = [&engine, i, &j]() {
        std::uniform_int_distribution<size_t> dist;
        return std::to_string(i) + "_" + std::to_string(dist(engine)) + "_" +
               std::to_string(j);
      };

      auto one_in_two = [&engine]() {
        std::uniform_int_distribution<size_t> dist;
        return dist(engine) & 1;
      };

      for (; j < kTestTimes; ++j) {
        auto k = gen_random_str();
        auto v = gen_random_str();
        auto code = kv->Set(k, v);
        if (code != ErrorCode::kOk) {
          __builtin_trap();
        }
        std_map[k] = v;
      }

      for (int k = 0; k < kTestTimes; ++k) {
        if (one_in_two()) {
          auto it = std_map.lower_bound(gen_random_str());
          if (it != std_map.end()) {
            auto code = kv->Del(it->first);
            if (code != ErrorCode::kOk) {
              __builtin_trap();
            }
            std_map.erase(it);
          }
        }
      }

      // auto it = std_map.begin();
      // auto code = kv->Scan(it->first, [&](const Slice& key, const Slice& val) {
      //   if (key != it->first) {
      //     __builtin_trap();
      //   }
      //   if (val != it->second) {
      //     __builtin_trap();
      //   }
      //   ++it;
      //   return it != std_map.end();
      // });

      // if (code != ErrorCode::kOk || it != std_map.end()) {
      //   __builtin_trap();
      // }
    });
  }

  for (auto& job : jobs) {
    job.join();
  }
  std::cout << "RunOpenHiKVTest." << std::endl;
}

}  // namespace open_hikv