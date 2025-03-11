#include "CPU.h"
#include "ISU.h"
#include <assert.h>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#ifdef CONFIG_PY
#include <pybind11/pybind11.h>
namespace py = pybind11;
#endif
using namespace std;

Inst_Entry decode(uint32_t inst);

// trace
uint32_t *br_trace;
uint32_t *mem_trace;
uint32_t *inst;
int btrace_num;
int mtrace_num;
int inst_num;

void CPU::init_trace() {
  streamsize size;
  char *ptr;
  // 加载分支trace
  ifstream btrace("./trace/core_btrace", ios::binary | ios::ate);
  size = btrace.tellg();
  btrace_num = size / sizeof(uint32_t);
  br_trace = new uint32_t[btrace_num];
  btrace.seekg(0, ios::beg);
  ptr = (char *)(br_trace);

  if (!btrace.read(ptr, size)) {
    cout << "branch trace read error" << endl;
    exit(1);
  }

  // 加载访存trace
  ifstream mtrace("./trace/core_mtrace", ios::binary | ios::ate);
  size = mtrace.tellg();
  mtrace_num = size / sizeof(uint32_t);
  mem_trace = new uint32_t[mtrace_num];
  mtrace.seekg(0, ios::beg);
  ptr = (char *)(mem_trace);

  if (!mtrace.read(ptr, size)) {
    cout << "memory trace read error" << endl;
    exit(1);
  }

  // 加载程序
  ifstream image("./trace/coremark.bin", ios::binary | ios::ate);
  size = image.tellg();
  inst_num = size / sizeof(uint32_t);
  image.seekg(0, ios::beg);
  inst = new uint32_t[inst_num];
  ptr = (char *)(inst);

  if (!image.read(ptr, size)) {
    cout << "program read error" << endl;
    exit(1);
  }
}

void CPU::init_cpu() { isu.init(); }

void CPU::reset() {
  isu.reset();
  br_idx = 0;
  mem_idx = 0;
  number_PC = 0x80000000;
  fetch_entry.clear();
  time = 0;
}

#ifdef CONFIG_PY
pair<float, vector<uint32_t>> CPU::step(pair<uint32_t, uint32_t> issue) {

  // 还有能发射的指令
  if (issue.first < 4) {
    pair<float, vector<uint32_t>> ret;
    if (isu.deq(issue)) {
      ret.first = 1;
    } else {
      ret.first = -10000;
      isu.sim_end = true;
    }
    ret.second = get_state();
    return ret;
  }
  time++;

  vector<list<Inst_Entry>::iterator> dispatch_it;
  for (auto it = fetch_entry.begin(); it != fetch_entry.end(); it++) {
    bool ret = isu.dispatch(*it);

    if (ret == false) {
      break;
    } else {
      dispatch_it.push_back(it);
    }
  }

  for (auto it : dispatch_it) {
    fetch_entry.erase(it);
  }

  bool br_taken;
  uint32_t next_pc;
  uint32_t instruction;
  Inst_Entry dec_inst;

  if (!isu.stall) {
    for (int i = 0; i < FETCH_WIDTH; i++) {
      instruction = inst[(number_PC - 0x80000000) >> 2];
      dec_inst = decode(instruction);
      dec_inst.pc = number_PC;

      if (dec_inst.type == LDU || dec_inst.type == STU) {
        dec_inst.addr = mem_trace[mem_idx];
        mem_idx++;
      }

      if (dec_inst.type == BRU) {
        // 检查分支预测是否正确
        isu.branch_num++;
        isu.bpu.bpu(number_PC, next_pc, br_taken);
        isu.bpu.bpu_update(number_PC, br_trace[br_idx],
                           br_trace[br_idx] != number_PC + 4);

        if (br_trace[br_idx] != next_pc) {
          next_pc = br_trace[br_idx];
          isu.stall = true;
          dec_inst.mispred = true;
          isu.mispred_num++;
        } else {
          dec_inst.mispred = false;
        }

        /*cout << hex << number_PC << " " << next_pc << endl;*/
        br_idx++;
      } else {
        next_pc = number_PC + 4;
        dec_inst.mispred = false;

        if (dec_inst.ebarek)
          isu.stall = true;
      }

      fetch_entry.push_back(dec_inst);

      number_PC = next_pc;
      if (isu.stall) {
        break;
      }
    }
  }

  isu.exec();

  pair<float, vector<uint32_t>> ret;
  ret.first = -1;
  ret.second = get_state();

  return ret;
}
#else
void CPU::step() {
  bool br_taken;
  uint32_t next_pc;
  uint32_t instruction;
  Inst_Entry dec_inst;

  if (!isu.sim_end) {
    if (!isu.stall) {
      for (int i = 0; i < FETCH_WIDTH; i++) {
        instruction = inst[(number_PC - 0x80000000) >> 2];
        dec_inst = decode(instruction);
        dec_inst.pc = number_PC;

        if (dec_inst.type == LDU || dec_inst.type == STU) {
          dec_inst.addr = mem_trace[mem_idx];
          mem_idx++;
        }

        if (dec_inst.type == BRU) {
          // 检查分支预测是否正确
          isu.branch_num++;
          isu.bpu.bpu(number_PC, next_pc, br_taken);
          isu.bpu.bpu_update(number_PC, br_trace[br_idx],
                             br_trace[br_idx] != number_PC + 4);

          if (br_trace[br_idx] != next_pc) {
            next_pc = br_trace[br_idx];
            isu.stall = true;
            dec_inst.mispred = true;
            isu.mispred_num++;
          } else {
            dec_inst.mispred = false;
          }

          /*cout << hex << number_PC << " " << next_pc << endl;*/
          br_idx++;
        } else {
          next_pc = number_PC + 4;
          dec_inst.mispred = false;

          if (dec_inst.ebarek)
            isu.stall = true;
        }

        fetch_entry.push_back(dec_inst);

        number_PC = next_pc;
        if (isu.stall) {
          break;
        }
      }
    }

    isu.exec();
#ifdef CONFIG_GREEDY
    for (int i = 0; i < 4; i++) {
      greedy_issue[i].clear();
    }
    brute_force(0);
    isu.deq(greedy_issue);
#else
    isu.deq();
#endif
    time++;

    vector<list<Inst_Entry>::iterator> dispatch_it;

    for (auto it = fetch_entry.begin(); it != fetch_entry.end(); it++) {
      bool ret = isu.dispatch(*it);

      if (ret == false) {
        break;
      } else {
        dispatch_it.push_back(it);
      }
    }

    for (auto it : dispatch_it) {
      fetch_entry.erase(it);
    }
  }
}
#endif

vector<uint32_t> CPU::get_state() {
  vector<uint32_t> state;
  state.push_back(isu.sim_end);
  for (auto iq : isu.iq_set) {
    for (int i = 0; i < iq.max_entry_num; i++) {
      if (i < iq.entry.size()) {
        list<Inst_Entry>::iterator it = iq.entry.begin();
        advance(it, i);
        state.push_back(it->dependency);
        state.push_back(it->mispred);
        state.push_back(i);
      } else {
        state.push_back(0);
        state.push_back(0);
        state.push_back(0);
      }
    }
  }

  return state;
}

bool CPU::end() { return isu.sim_end || (time >= MAX_SIM_TIME); }

void CPU::print_prf() {
  if (time == MAX_SIM_TIME) {
    cout << "TIME OUT" << endl;
  } else {
    cout << "#######################" << endl;
    cout << "SUCCESS!!!" << endl;
    cout << "CYCLE: " << dec << time << endl;
    cout << "INST : " << dec << isu.commit_num << endl;
    cout << "IPC  : " << (double)isu.commit_num / time << endl;
    cout << "BRANCH : " << dec << isu.branch_num << endl;
    cout << "MISPRED: " << dec << isu.mispred_num << endl;
    cout << "CACHE HIT : " << dec
         << isu.cache.cache_access - isu.cache.cache_miss << endl;
    cout << "CACHE MISS: " << dec << isu.cache.cache_miss << endl;
  }
}

vector<vector<uint32_t>> combinations(vector<uint32_t> &nums, int n) {
  vector<vector<uint32_t>> result;
  int m = nums.size();
  if (n <= 0 || n > m)
    return result;

  // 生成所有二进制掩码，包含恰好 n 个 1
  for (int mask = (1 << n) - 1; mask < (1 << m);) {
    vector<uint32_t> comb;
    for (int i = 0; i < m; ++i) {
      if (mask & (1 << i)) {
        comb.push_back(nums[i]);
      }
    }
    if (!comb.empty())
      result.push_back(comb);

    // 计算下一个有效掩码
    int t = mask | (mask - 1);
    mask = (t + 1) | (((~t & -~t) - 1) >> (__builtin_ctz(mask) + 1));
  }
  return result;
}

int CPU::brute_force(int depth) {

  vector<vector<uint32_t>> idle_fu_idx(4);
  vector<vector<uint32_t>> ready_issue(4);
  // 查看各类型待发射指令个数
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < isu.iq_set[i].entry.size(); j++) {
      list<Inst_Entry>::iterator it = isu.iq_set[i].entry.begin();
      advance(it, j);

      if ((!it->src1_en || !it->src1_busy) &&
          (!it->src2_en || !it->src2_busy) &&
          (it->type != LDU || it->pre_store_num == 0)) {
        ready_issue[i].push_back(j);
      }
    }
  }

  if (ready_issue[0].empty() && ready_issue[1].empty() &&
      ready_issue[2].empty() && ready_issue[3].empty()) {
    return 0;
  }

  bool no_issue = true;
  // 查看各类型空闲的执行单元个数
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < isu.iq_set[i].fu_num; j++) {
      if (isu.iq_set[i].fu_set[j].ready) {
        idle_fu_idx[i].push_back(j);
      }
    }
    no_issue = no_issue && (ready_issue[i].empty() || idle_fu_idx[i].empty());
  }

  while (no_issue) {
    isu.exec();
    for (int i = 0; i < 4; i++) {
      idle_fu_idx[i].clear();
      for (int j = 0; j < isu.iq_set[i].fu_num; j++) {
        if (isu.iq_set[i].fu_set[j].ready) {
          idle_fu_idx[i].push_back(j);
        }
      }

      no_issue = no_issue && (ready_issue[i].empty() || idle_fu_idx[i].empty());
    }
  }
  // 遍历所有可能的发射组合
  vector<vector<uint32_t>> comb_issue[4];
  for (int i = 0; i < 4; i++) {
    if (idle_fu_idx[i].size() >= ready_issue[i].size()) {
      comb_issue[i] = {ready_issue[i]};
    } else if (idle_fu_idx[i].size() == 0) {
      comb_issue[i] = {vector<uint32_t>(0)};
    } else {
      comb_issue[i] = combinations(ready_issue[i], idle_fu_idx[i].size());
    }
  }

  int min_T = 10000;
  vector<vector<uint32_t>> all_issue(4);
  for (int i = 0; i < comb_issue[0].size(); i++) {
    all_issue[0] = comb_issue[0][i];
    for (int j = 0; j < comb_issue[1].size(); j++) {
      all_issue[1] = comb_issue[1][j];
      for (int k = 0; k < comb_issue[2].size(); k++) {
        all_issue[2] = comb_issue[2][k];
        for (int m = 0; m < comb_issue[3].size(); m++) {
          all_issue[3] = comb_issue[3][m];
          CPU test_cpu = *this;
          test_cpu.isu.deq(all_issue);
          test_cpu.isu.exec();
          int T = test_cpu.brute_force(depth + 1) + 1;
          if (T < min_T) {
            min_T = T;
            // 记录当前的发射指令
            if (depth == 0) {
              greedy_issue = all_issue;
            }
          }
        }
      }
    }
  }

  return min_T;
}

#ifdef CONFIG_PY
PYBIND11_MODULE(cpu_simulator, m) {
  py::class_<CPU>(m, "CPU")
      .def(py::init<>())
      .def("init_trace", &CPU::init_trace)
      .def("init_cpu", &CPU::init_cpu)
      .def("reset", &CPU::reset)
      .def("get_state", &CPU::get_state)
      .def("step", &CPU::step)
      .def("end", &CPU::end)
      .def("print_prf", &CPU::print_prf);
}
#endif
