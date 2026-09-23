#pragma once
#define MALLOC_CAP_8BIT 1
inline unsigned heap_caps_get_largest_free_block(int) { return 100000; }
