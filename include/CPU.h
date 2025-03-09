#pragma once
#include "ISU.h"
#include <cstdint>
#define CONFIG_PY

class CPU {
public:
  ISU isu;
  int br_idx = 0;
  int mem_idx = 0;
  uint32_t number_PC = 0x80000000;
  list<Inst_Entry> fetch_entry;

  void init_trace();
  void init_cpu();
  void reset();
  /*vector<int> get_state();*/
  void get_state();
  /*pair<float, vector<int>> step(vector<int> action);*/
  void step();
  void print_prf();
  bool end();

  int time = 0;
};
