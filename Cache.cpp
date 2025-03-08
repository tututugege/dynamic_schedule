#include "Cache.h"
#include <cstdlib>

extern int cache_access;
extern int cache_miss;

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
