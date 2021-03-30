#pragma once

constexpr unsigned long long operator""_KB(unsigned long long v) {
  return v << 10;
}

constexpr unsigned long long operator""_MB(unsigned long long v) {
  return v << 20;
}

constexpr unsigned long long operator""_GB(unsigned long long v) {
  return v << 30;
}

constexpr unsigned long long operator""_TB(unsigned long long v) {
  return v << 40;
}