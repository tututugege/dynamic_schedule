#include "BPU.h"
#include <assert.h>
#include <cstdint>

void BPU::reset() {
  for (int i = 0; i < (1 << BHR_WIDTH); i++) {
    counter[i] = 0;
    BTB[i] = 0;
  }
}

void BPU::bpu(uint32_t pc, uint32_t &pc_next, bool &br_taken) {
  int idx = ((pc >> 2) & ((1 << BHR_WIDTH) - 1));
  br_taken = counter[idx] >= 2;
  if (br_taken) {
    pc_next = BTB[idx];
  } else {
    pc_next = pc + 4;
  }
}

void BPU::bpu_update(uint32_t pc, uint32_t br_pc, bool br_taken) {
  // 更新
  int idx = ((pc >> 2) & ((1 << BHR_WIDTH) - 1));
  if (br_taken) {
    BTB[idx] = br_pc;
    if (counter[idx] != 3) {
      counter[idx]++;
    }
  } else {
    if (counter[idx] != 0) {
      counter[idx]--;
    }
  }
}
