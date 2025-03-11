#include "./CPU.h"
#include <cstdint>
#include <iostream>

int main() {

  CPU cpu;
  cpu.init_trace();
  cpu.init_cpu();
  cpu.reset();

  while (!cpu.end()) {
    /*if (LOG) {*/
    /*if (cpu.time % 1000 == 0) {*/
    /*  cout << "********************" << endl;*/
    /*  cout << "-- TIME: " << dec << cpu.time << " --" << endl;*/
    /*}*/
    /*}*/

    cpu.step();
  }

  cpu.print_prf();

  return 0;
}

Inst_Entry decode(uint32_t inst) {
  if (inst == INST_EBREAK) {
    Inst_Entry entry;
    entry.dest_en = false;
    entry.src1_en = false;
    entry.src2_en = false;
    entry.type = ALU;
    entry.instruction = inst;
    entry.ebarek = true;
    return entry;
  }

  // 操作数来源以及type
  bool dest_en, src1_en, src2_en;
  Fu_Type type;
  bool inst_bit[32];
  cvt_number_to_bit_unsigned(inst_bit, inst, 32);

  // split instruction
  bool *bit_op_code = inst_bit + 25; // 25-31
  bool *rd_code = inst_bit + 20;     // 20-24
  bool *rs_a_code = inst_bit + 12;   // 12-16
  bool *rs_b_code = inst_bit + 7;    // 7-11

  // 准备opcode、funct3、funct7
  uint32_t number_op_code_unsigned = cvt_bit_to_number_unsigned(bit_op_code, 7);

  // 准备寄存器
  int reg_d_index = cvt_bit_to_number_unsigned(rd_code, 5);
  int reg_a_index = cvt_bit_to_number_unsigned(rs_a_code, 5);
  int reg_b_index = cvt_bit_to_number_unsigned(rs_b_code, 5);

  switch (number_op_code_unsigned) {
  case number_0_opcode_lui:     // lui
  case number_1_opcode_auipc: { // auipc
    dest_en = true;
    src1_en = false;
    src2_en = false;
    type = ALU;
    break;
  }

  case number_2_opcode_jal: { // jal
    dest_en = true;
    src1_en = false;
    src2_en = false;
    type = BRU;
    break;
  }
  case number_3_opcode_jalr: { // jalr
    dest_en = true;
    src1_en = true;
    src2_en = false;
    type = BRU;
    break;
  }
  case number_4_opcode_beq: { // beq, bne, blt, bge, bltu, bgeu
    dest_en = false;
    src1_en = true;
    src2_en = true;
    type = BRU;
    break;
  }
  case number_5_opcode_lb: { // lb, lh, lw, lbu, lhu
    dest_en = true;
    src1_en = true;
    src2_en = false;
    type = LDU;
    break;
  }
  case number_6_opcode_sb: { // sb, sh, sw
    dest_en = false;
    src1_en = true;
    src2_en = true;
    type = STU;
    break;
  }
  case number_7_opcode_addi: { // addi, slti, sltiu, xori, ori, andi, slli,
    dest_en = true;
    src1_en = true;
    src2_en = false;
    type = ALU;
    break;
  }
  case number_8_opcode_add: { // add, sub, sll, slt, sltu, xor, srl, sra, or,
    dest_en = true;
    src1_en = true;
    src2_en = true;
    type = ALU;
    break;
  }
  case number_9_opcode_fence: { // fence, fence.i
    dest_en = false;
    src1_en = false;
    src2_en = false;
    type = ALU;
    break;
  }

  default: {
    cerr << "Error: unknown instruction: ";
    cerr << hex << inst << endl;
    /*assert(0);*/
    break;
  }
  }

  // 不写0寄存器
  if (reg_d_index == 0)
    dest_en = false;

  Inst_Entry entry;
  entry.dest_en = dest_en;
  entry.src1_en = src1_en;
  entry.src2_en = src2_en;
  entry.src1_areg = reg_a_index;
  entry.src2_areg = reg_b_index;
  entry.dest_areg = reg_d_index;
  entry.type = type;
  entry.instruction = inst;
  entry.ebarek = false;

  return entry;
}
