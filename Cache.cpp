#include "Cache.h"
#include <cstdlib>

void Cache::reset() {
  cache_access = 0;
  cache_miss = 0;
  for (int i = 0; i < way_num; i++) {
    for (int j = 0; j < (1 << index_width); j++) {
      cache_valid[i][j] = false;
      cache_tag[i][j] = 0;
    }
  }
}
int Cache::cache_select_evict() { return rand() % way_num; }

// miss
void Cache::cache_evict(uint32_t addr) {
  int evicit_way = cache_select_evict();
  cache_tag[evicit_way][INDEX(addr)] = TAG(addr);
  cache_valid[evicit_way][INDEX(addr)] = true;
}

int Cache::cache_read(uint32_t addr) {
  cache_access++;
  uint32_t tag;
  int i;

  for (i = 0; i < way_num; i++) {
    tag = cache_tag[i][INDEX(addr)];
    if (cache_valid[i][INDEX(addr)] && tag == TAG(addr)) {
      break;
    }
  }

  if (i == way_num) {
    cache_evict(addr);
    cache_miss++;
    return MISS_LATENCY;
  }

  return HIT_LATENCY;
}
