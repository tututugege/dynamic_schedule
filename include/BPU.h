#include <cstdint>
#define BHR_WIDTH 8

class BPU {
  uint8_t counter[1 << BHR_WIDTH];
  uint32_t BTB[1 << BHR_WIDTH];

public:
  void bpu(uint32_t pc, uint32_t &pc_next, bool &br_taken);
  void bpu_update(uint32_t pc, uint32_t br_pc, bool br_taken);
};
