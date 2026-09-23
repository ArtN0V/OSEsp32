#pragma once
#include <algorithm>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <cstdio>
inline void delay(unsigned) {}
using std::min;
extern uint32_t testMillis;
inline uint32_t millis() { return testMillis; }
inline size_t strlcpy(char* dst, const char* src, size_t n) {
  size_t length = strlen(src);
  if (n) { size_t copy = std::min(n - 1, length); memcpy(dst, src, copy); dst[copy] = 0; }
  return length;
}
struct TestESP { uint32_t getFreeHeap() const { return 200000; } };
extern TestESP ESP;
