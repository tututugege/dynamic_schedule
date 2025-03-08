#include "./BPU.h"
#include "./Cache.h"
#include "./IQ.h"
#include "./ISU.h"
#include <cstdint>
#include <fstream>
#include <iostream>

typedef struct {
  uint32_t br_pc;
} Btrace_Node;

typedef struct {
  uint32_t addr;
} Mtrace_Node;

ISU isu;
BPU bpu;
Cache cache;

int br_idx = 0;
int mem_idx = 0;
bool sim_end = false;
bool stall = false;
int commit_num = 0;
int branch_num = 0;
int mispred_num = 0;
int cache_access = 0;
int cache_miss = 0;

Inst_Entry decode(uint32_t inst);

int main() {

  streamsize size;
  char *ptr;
  // 加载分支trace
  ifstream btrace("./trace/core_btrace", ios::binary | ios::ate);
  size = btrace.tellg();
  int btrace_num = size / sizeof(Btrace_Node);
  Btrace_Node br_trace[btrace_num];
  btrace.seekg(0, ios::beg);
  ptr = (char *)(&br_trace);

  if (!btrace.read(ptr, size)) {
    cout << "branch trace read error" << endl;
    exit(1);
  }

  // 加载访存trace
  ifstream mtrace("./trace/core_mtrace", ios::binary | ios::ate);
  size = mtrace.tellg();
  int mtrace_num = size / sizeof(Mtrace_Node);
  Mtrace_Node mem_trace[mtrace_num];
  mtrace.seekg(0, ios::beg);
  ptr = (char *)(&mem_trace);

  if (!mtrace.read(ptr, size)) {
    cout << "memory trace read error" << endl;
    exit(1);
  }

  // 加载程序
  ifstream image("./trace/coremark.bin", ios::binary | ios::ate);
  size = image.tellg();
  int inst_num = size / sizeof(uint32_t);
  image.seekg(0, ios::beg);
  uint32_t inst[inst_num];
  ptr = (char *)(&inst);

  if (!image.read(ptr, size)) {
    cout << "program read error" << endl;
    exit(1);
  }

  isu.reset();

  int time = 0;

  uint32_t number_PC = 0x80000000;
  list<Inst_Entry> fetch_entry;
  bool br_taken;
  uint32_t next_pc;
  uint32_t instruction;
  Inst_Entry dec_inst;

  while (!sim_end) {
    if (LOG) {
      cout << "********************" << endl;
      cout << "-- TIME: " << dec << time++ << " --" << endl;
    }

    if (!stall) {
      for (int i = 0; i < FETCH_WIDTH; i++) {
        instruction = inst[(number_PC - 0x80000000) >> 2];
        dec_inst = decode(instruction);
        dec_inst.pc = number_PC;

        if (dec_inst.type == LDU || dec_inst.type == STU) {
          dec_inst.addr = mem_trace[mem_idx].addr;
          mem_idx++;
        }

        if (dec_inst.type == BRU) {
          // 检查分支预测是否正确
          branch_num++;
          bpu.bpu(number_PC, next_pc, br_taken);
          bpu.bpu_update(number_PC, br_trace[br_idx].br_pc,
                         br_trace[br_idx].br_pc != number_PC + 4);

          if (br_trace[br_idx].br_pc != next_pc) {
            next_pc = br_trace[br_idx].br_pc;
            stall = true;
            dec_inst.mispred = true;
            mispred_num++;
          } else {
            dec_inst.mispred = false;
          }

          /*cout << hex << number_PC << " " << next_pc << endl;*/
          br_idx++;
        } else {
          next_pc = number_PC + 4;
          dec_inst.mispred = false;

          if (dec_inst.ebarek)
            stall = true;
        }

        fetch_entry.push_back(dec_inst);

        number_PC = next_pc;
        if (stall) {
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

  if (time == MAX_SIM_TIME) {
    cout << "TIME OUT" << endl;
  } else {

    while (!isu.is_empty()) {
      isu.deq();
      isu.exec();
      time++;
    }

    cout << "#######################" << endl;
    cout << "SUCCESS!!!" << endl;
    cout << "CYCLE: " << dec << time << endl;
    cout << "INST : " << dec << commit_num << endl;
    cout << "IPC  : " << (double)commit_num / time << endl;
    cout << "BRANCH : " << dec << branch_num << endl;
    cout << "MISPRED: " << dec << mispred_num << endl;
    cout << "CACHE HIT : " << dec << cache_access - cache_miss << endl;
    cout << "CACHE MISS: " << dec << cache_miss << endl;
  }

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
