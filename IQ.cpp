#include <IQ.h>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <vector>

Inst_Entry decode(uint32_t inst);

int fu_num[FU_TYPE_NUM] = {4, 1, 1, 1};

void IQ::init() {
  for (int i = 0; i < FU_TYPE_NUM; i++) {
    fu_set[i] = new FU[fu_num[i]];
    for (int j = 0; j < fu_num[i]; j++) {
      fu_set[i][j].ready = true;
    }
  }

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

void IQ::rename(Inst_Entry &inst) {
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

void IQ::enq(uint32_t pc, uint32_t inst) {
  Inst_Entry new_entry = decode(inst);
  new_entry.pc = pc;
  new_entry.pre_store_num = 0;
  if (LOG)
    cout << "派遣指令" << hex << inst << endl;

  rename(new_entry);
  if (new_entry.type == LDU) {
    for (auto e : entry) {
      if (e.type == STU) {
        new_entry.pre_store_num++;
      }
    }

    for (int i = 0; i < fu_num[STU]; i++) {
      if (!fu_set[STU][i].ready) {
        new_entry.pre_store_num++;
      }
    }
  }

  entry.push_back(new_entry);
}

void IQ::awake(uint32_t dest_preg) {
  for (auto &e : entry) {
    if (e.src1_en && e.src1_preg == dest_preg) {
      e.src1_busy = false;
    }

    if (e.src2_en && e.src2_preg == dest_preg) {
      e.src2_busy = false;
    }
  }
}

void IQ::issue(list<Inst_Entry>::iterator it, FU *fu) {

  fu->ready = false;
  fu->inst = *it;
  if (it->type == ALU || it->type == BRU) {
    fu->latency = 1;
  } else if (it->type == LDU) {
    fu->latency = 2;
  } else if (it->type == STU) {
    fu->latency = 5;
  }

  if (LOG)
    cout << "发射指令" << hex << it->instruction << endl;

  if (it->dest_en) {
    awake(it->dest_preg);
  }
}

void IQ::scheduler(Fu_Type type) {

  int idle_fu_num = 0;
  vector<int> idle_fu_idx;
  for (int i = 0; i < fu_num[type]; i++) {
    if (fu_set[type][i].ready) {
      idle_fu_idx.push_back(i);
      idle_fu_num++;
    }
  }

  int issue_num = 0;
  vector<list<Inst_Entry>::iterator> issue_it;

  for (auto it = entry.begin(); it != entry.end();) {
    if (issue_num == idle_fu_num)
      break;

    if (it->type == type && (!it->src1_en || !it->src1_busy) &&
        (!it->src2_en || !it->src2_busy) &&
        (type != LDU || it->pre_store_num == 0)) {
      issue(it, &fu_set[type][idle_fu_idx[issue_num++]]);
      issue_it.push_back(it);
    }

    it++;
  }

  for (auto it : issue_it) {
    entry.erase(it);
  }
}

void IQ::deq() {
  for (int i = 0; i < FU_TYPE_NUM; i++) {
    scheduler((Fu_Type)i);
  }
}

void IQ::exec() {
  for (int i = 0; i < FU_TYPE_NUM; i++) {
    for (int j = 0; j < fu_num[i]; j++) {
      if (fu_set[i][j].ready == false) {
        fu_set[i][j].latency--;

        // 执行完毕
        if (fu_set[i][j].latency == 0) {
          fu_set[i][j].ready = true;

          if (fu_set[i][j].inst.dest_en) {
            busy_table[fu_set[i][j].inst.dest_preg] = false;
            idle_reg.push_back(fu_set[i][j].inst.old_dest_preg);
          }

          if (LOG)
            cout << "指令完毕" << hex << fu_set[i][j].inst.instruction << endl;

          if (i == STU) {
            for (auto &e : entry) {
              if (e.type == LDU && e.pre_store_num != 0) {
                e.pre_store_num--;
              }
            }
          }
        }
      }
    }
  }
  /*for (auto i : idle_reg) {*/
  /*  cout << i << " ";*/
  /*}*/
  /*cout << endl;*/
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

bool IQ::is_full() { return entry.size() >= MAX_IQ_ENTRY_NUM; }
bool IQ::is_empty() { return entry.size() == 0; }

void IQ::print() {
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
