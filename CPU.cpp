#include "CPU.h"
#include <cstdint>
#include <fstream>
#include <iostream>

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
    isu.deq();
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

void CPU::get_state() {}

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
