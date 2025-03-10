#pragma once
#include <cstdint>
using namespace std;

#define OFFSET_WIDTH 5
#define INDEX_WIDTH 8
#define WAY_NUM 1

#define TAG(addr) (addr >> (offset_width + index_width))
#define INDEX(addr) ((addr >> offset_width) & ((1 << index_width) - 1))

#define HIT_LATENCY 3
#define MISS_LATENCY 100

class Cache {
  uint32_t cache_tag[WAY_NUM][1 << INDEX_WIDTH];
  uint32_t cache_valid[WAY_NUM][1 << INDEX_WIDTH];

  int offset_width = OFFSET_WIDTH;
  int index_width = INDEX_WIDTH;
  int way_num = WAY_NUM;

public:
  void reset();
  int cache_select_evict();
  void cache_evict(uint32_t addr);
  int cache_read(uint32_t addr); // 返回延迟
  int cache_access = 0;
  int cache_miss = 0;
};
