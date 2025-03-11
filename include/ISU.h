#pragma once
#include <BPU.h>
#include <Cache.h>
#include <cstdint>
#include <iostream>
#include <list>
#include <vector>

#define INST_EBREAK 0x00100073
#define MAX_PREG 96
#define MAX_SIM_TIME 1000000
#define FETCH_WIDTH 4
/*#define CONFIG_GREEDY*/
/*#define CONFIG_OLDEST_FIRST*/
/*#define CONFIG_MAX_DEPEND*/
/*#define CONFIG_LONG_DEPEND*/
/*#define CONFIG_BR_DEPEND*/

using namespace std;

#define LOG 0

enum enum_number_opcode {
  number_0_opcode_lui = 0b0110111,   // lui
  number_1_opcode_auipc = 0b0010111, // auipc
  number_2_opcode_jal = 0b1101111,   // jal
  number_3_opcode_jalr = 0b1100111,  // jalr
  number_4_opcode_beq = 0b1100011,   // beq, bne, blt, bge, bltu, bgeu
  number_5_opcode_lb = 0b0000011,    // lb, lh, lw, lbu, lhu
  number_6_opcode_sb = 0b0100011,    // sb, sh, sw
  number_7_opcode_addi =
      0b0010011, // addi, slti, sltiu, xori, ori, andi, slli, srli, srai
  number_8_opcode_add =
      0b0110011, // add, sub, sll, slt, sltu, xor, srl, sra, or, and
  number_9_opcode_fence = 0b0001111, // fence, fence.i
  number_10_opcode_ecall =
      0b1110011, // ecall, ebreak, csrrw, csrrs, csrrc, csrrwi, csrrsi, csrrci
  number_11_opcode_lrw =
      0b0101111, // lr.w, sc.w, amoswap.w, amoadd.w, amoxor.w, amoand.w,
                 // amoor.w, amomin.w, amomax.w, amominu.w, amomaxu.w
};

inline long cvt_bit_to_number_unsigned(bool input_data[], int k) {

  uint32_t output_number = 0;
  int i;
  for (i = 0; i < k; i++) {
    output_number += uint32_t(input_data[i]) << (k - 1 - i);
  }
  return output_number;
};

void inline cvt_number_to_bit_unsigned(bool *output_number, uint32_t input_data,
                                       int k) {
  int i;
  for (i = 0; i < k; i++) {
    output_number[i] = (input_data >> (k - 1 - i)) & uint32_t(1);
  }
};

enum Fu_Type { ALU, LDU, STU, BRU, FU_TYPE_NUM };

class Inst_Entry {
public:
  bool valid;
  uint32_t pc;
  uint32_t instruction;
  Fu_Type type;
  uint32_t src1_areg, src2_areg, dest_areg;
  uint32_t src1_preg, src2_preg, dest_preg, old_dest_preg;
  bool src1_en, src2_en, dest_en;
  bool src1_busy, src2_busy;
  uint32_t addr;
  int pre_store_num;
  bool mispred;
  bool ebarek;

  // 分支信息
  uint32_t br_pc;
  bool br_taken;

  // 调度信息
  int dependency = 0;
};

class FU {
public:
  bool ready;
  uint32_t latency = 0;
  Fu_Type type;
  Inst_Entry inst;
};

class ISU {

public:
  void init();
  void reset();
  bool is_empty();
  bool dispatch(Inst_Entry e);
  void deq();
  bool deq(pair<uint32_t, uint32_t>);
  void deq(vector<vector<uint32_t>> idx);
  void print();
  void exec();
  void rename(Inst_Entry &inst);
  void awake(uint32_t dest_preg);
  void store_awake();

  vector<bool> busy_table;
  vector<uint32_t> idle_reg;
  vector<uint32_t> rename_table;

  BPU bpu;
  Cache cache;
  bool sim_end;
  bool stall;

  // 性能统计
  int commit_num = 0;
  int branch_num = 0;
  int mispred_num = 0;

  class IQ {
  public:
    /*CPU *parent_cpu;*/
    Fu_Type type;
    int fu_num;
    int max_entry_num;
    list<Inst_Entry> entry;
    vector<FU> fu_set;

    void init(Fu_Type type, int entry_num, int fu_num) {
      this->type = type;
      this->fu_num = fu_num;
      this->max_entry_num = entry_num;
      fu_set.resize(fu_num);

      for (int i = 0; i < fu_num; i++) {
        fu_set[i].type = type;
        fu_set[i].ready = true;
      }
    }

    void reset() {
      for (int i = 0; i < fu_num; i++) {
        fu_set[i].ready = true;
      }
      entry.clear();
    }

    void enq(Inst_Entry new_entry) { entry.push_back(new_entry); }

    void add_depend(uint32_t preg) {
      for (auto &e : entry) {
        if (e.dest_en && e.dest_preg == preg) {
          e.dependency++;
#if defined(CONFIG_LONG_DEPEND) || defined(CONFIG_BR_DEPEND)
          if (e.src1_en && e.src1_busy) {
            add_depend(e.src1_preg);
          }

          if (e.src2_en && e.src2_busy) {
            add_depend(e.src2_preg);
          }
#endif
        }
      }
    }

    void awake(uint32_t dest_preg) {
      for (auto &e : entry) {
        if (e.src1_en && e.src1_preg == dest_preg) {
          e.src1_busy = false;
        }

        if (e.src2_en && e.src2_preg == dest_preg) {
          e.src2_busy = false;
        }
      }
    }

    void issue(list<Inst_Entry>::iterator it, FU *fu, ISU *isu) {
      fu->ready = false;
      fu->inst = *it;
      if (it->type == ALU || it->type == BRU) {
        fu->latency = 1;
      } else if (it->type == LDU) {
        fu->latency = isu->cache.cache_read(it->addr);
      } else if (it->type == STU) {
        fu->latency = isu->cache.cache_read(it->addr);
      }

      if (LOG) {
        cout << "发射指令" << hex << it->instruction << endl;
      }

      if (it->dest_en && it->type != LDU) {
        isu->awake(it->dest_preg);
      }
    }

    void deq(ISU *isu) {
      int idle_fu_num = 0;
      vector<int> idle_fu_idx;
      for (int i = 0; i < fu_num; i++) {
        if (fu_set[i].ready) {
          idle_fu_idx.push_back(i);
          idle_fu_num++;
        }
      }

      int issue_num = 0;
      vector<list<Inst_Entry>::iterator> issue_it;

#ifdef CONFIG_OLDEST_FIRST
      for (auto it = entry.begin(); it != entry.end();) {
        if (issue_num == idle_fu_num)
          break;

        if (it->type == type && (!it->src1_en || !it->src1_busy) &&
            (!it->src2_en || !it->src2_busy) &&
            (type != LDU || it->pre_store_num == 0)) {
          issue(it, &fu_set[idle_fu_idx[issue_num++]], isu);
          issue_it.push_back(it);
        }

        it++;
      }

      for (auto it : issue_it) {
        entry.erase(it);
      }

#elif defined(CONFIG_MAX_DEPEND) || defined(CONFIG_LONG_DEPEND) ||             \
    defined(CONFIG_BR_DEPEND)
      while (issue_num < idle_fu_num) {
        int max_dependency = -1;
        list<Inst_Entry>::iterator max_dep_it = entry.end();
        for (auto it = entry.begin(); it != entry.end(); it++) {
          if (it->dependency > max_dependency) {
            max_dependency = it->dependency;
            max_dep_it = it;
          }
        }

        if (max_dep_it != entry.end()) {
          issue(max_dep_it, &fu_set[idle_fu_idx[issue_num++]], isu);
          entry.erase(max_dep_it);
        } else {
          break;
        }
      }

#endif
    }

    void exec(ISU *isu) {
      for (int i = 0; i < fu_num; i++) {
        if (fu_set[i].ready == false) {
          fu_set[i].latency--;

          // 执行完毕
          if (fu_set[i].latency == 0) {
            fu_set[i].ready = true;

            // EBREAK
            if (fu_set[i].inst.ebarek)
              isu->sim_end = true;

            // 检查出分支预测错误
            if (fu_set[i].type == BRU && fu_set[i].inst.mispred) {
              isu->stall = false;
            }

            if (fu_set[i].inst.dest_en) {
              isu->idle_reg.push_back(fu_set[i].inst.old_dest_preg);
              if (fu_set[i].inst.type == LDU) {
                isu->awake(fu_set[i].inst.dest_preg);
              }
            }

            if (LOG)
              cout << "指令完毕" << hex << fu_set[i].inst.instruction << endl;
            if (type == STU) {
              isu->store_awake();
            }
            isu->commit_num++;
          }
        }
      }
      /*for (auto i : idle_reg) {*/
      /*  cout << i << " ";*/
      /*}*/
      /*cout << endl;*/
    }

    bool is_full() { return entry.size() >= max_entry_num; }
    bool is_empty() { return entry.size() == 0; }

    void print() {
      cout << "----" << endl;
      for (auto e : entry) {
        printf("%x src1: %2x busy: %d src2: %2x, busy: %d, dest: %2x", e.pc,
               e.src1_preg, e.src1_busy, e.src2_preg, e.src2_busy, e.dest_preg);

        if (e.type == STU) {
          printf(" store\n");
        } else if (e.type == LDU) {
          printf(" load prestore %d\n", e.pre_store_num);
        } else if (e.type == BRU) {
          printf(" branch\n");
        } else {
          printf(" alu\n");
        }
      }
    }
  };

  vector<IQ> iq_set = vector<IQ>(FU_TYPE_NUM);
};
