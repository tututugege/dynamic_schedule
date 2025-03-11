#pragma once
#include "ISU.h"
#include <cstdint>
#include <vector>
/*#define CONFIG_PY*/

class CPU {
public:
  ISU isu;
  int br_idx = 0;
  int mem_idx = 0;
  uint32_t number_PC = 0x80000000;
  list<Inst_Entry> fetch_entry;
  vector<vector<uint32_t>> greedy_issue = vector<vector<uint32_t>>(4);

  void init_trace();
  void init_cpu();
  void reset();
  vector<uint32_t> get_state();
  int brute_force(int depth);
  void step();

  pair<float, vector<uint32_t>> step(pair<uint32_t, uint32_t> issue);
  void print_prf();
  bool end();

  int time = 0;
};
