#pragma once

#include <cstdint>
#include <list>

#define INST_EBREAK 0x00100073
#define MAX_PREG 96
#define MAX_SIM_TIME 1000000
#define FETCH_WIDTH 4
/*#define CONFIG_OLDEST_FIRST*/
/*#define CONFIG_MAX_DEPEND*/
#define CONFIG_LONG_DEPEND

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

class IQ {
public:
  Fu_Type type;
  int fu_num;
  int max_entry_num;
  list<Inst_Entry> entry;
  FU *fu_set;
  void init(Fu_Type type, int entry_num, int fu_num);
  bool is_full();
  bool is_empty();
  void enq(Inst_Entry new_entry);
  void scheduler();
  void issue(list<Inst_Entry>::iterator it, FU *fu);
  void deq();
  void exec();
  void print();
  void awake(uint32_t dest_preg);
  void add_depend(uint32_t preg);
};
