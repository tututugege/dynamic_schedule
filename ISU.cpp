#include "ISU.h"
#include "IQ.h"
#include <cstdint>
#include <iostream>

Inst_Entry decode(uint32_t inst);

void ISU::reset() {
  iq_set[0].init(ALU, 32, 4);
  iq_set[1].init(LDU, 16, 1);
  iq_set[2].init(STU, 8, 1);
  iq_set[3].init(BRU, 8, 1);

  for (int i = 0; i < MAX_PREG; i++) {
    busy_table.push_back(false);
  }

  for (int i = 32; i < MAX_PREG; i++) {
    idle_reg.push_back(i);
  }

  for (int i = 0; i < 32; i++) {
    rename_table.push_back(i);
  }
}

void ISU::rename(Inst_Entry &inst) {
  // rename
  inst.src1_preg = rename_table[inst.src1_areg];
  inst.src2_preg = rename_table[inst.src2_areg];
  inst.old_dest_preg = rename_table[inst.dest_areg];

  inst.src1_busy = busy_table[inst.src1_preg];
  inst.src2_busy = busy_table[inst.src2_preg];

  if (inst.dest_en) {
    if (idle_reg.size() == 0) {
      cout << "Error: no idle register" << endl;
      exit(1);
    }
    inst.dest_preg = idle_reg.back();
    idle_reg.pop_back();
    busy_table[inst.dest_preg] = true;
    rename_table[inst.dest_areg] = inst.dest_preg;
  }
}

bool ISU::dispatch(uint32_t pc, uint32_t inst) {
  Inst_Entry new_entry = decode(inst);
  if (LOG)
    cout << "派遣指令" << hex << inst << endl;

  new_entry.pc = pc;
  new_entry.pre_store_num = 0;
  rename(new_entry);

  for (auto &iq : iq_set) {
    if (iq.type == new_entry.type) {
      if (iq.is_full())
        return false;

      if (new_entry.type == LDU) {
        new_entry.pre_store_num = iq_set[STU].entry.size();

        for (int i = 0; i < iq_set[STU].fu_num; i++) {
          if (!iq_set[STU].fu_set[i].ready) {
            new_entry.pre_store_num++;
          }
        }
      }
      iq.enq(new_entry);
      break;
    }
  }

  return true;
}

Inst_Entry decode(uint32_t inst) {
  if (inst == INST_EBREAK) {
    Inst_Entry entry;
    entry.dest_en = false;
    entry.src1_en = false;
    entry.src2_en = false;
    entry.type = ALU;
    entry.instruction = inst;
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

  return entry;
}

void ISU::awake(uint32_t dest_preg) {
  busy_table[dest_preg] = false;
  for (auto &iq : iq_set)
    iq.awake(dest_preg);
}

void ISU::exec() {
  for (auto &iq : iq_set)
    iq.exec();
}

void ISU::deq() {
  for (auto &iq : iq_set)
    iq.deq();
}

void ISU::store_awake() {
  for (auto &e : iq_set[LDU].entry) {
    if (e.pre_store_num != 0)
      e.pre_store_num--;
  }
}

bool ISU::is_empty() {
  bool empty = true;
  for (auto &iq : iq_set)
    empty &= iq.is_empty();

  return empty;
}

void ISU::print() {
  for (auto &iq : iq_set)
    iq.print();

  for (int i = 0; i < busy_table.size(); i++) {
    cout << hex << i << ": ";
    cout << busy_table[i] << ": " << endl;
  }
}
