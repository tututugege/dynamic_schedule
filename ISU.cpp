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
  if (inst.src1_en) {
    inst.src1_preg = rename_table[inst.src1_areg];
    inst.src1_busy = busy_table[inst.src1_preg];
  } else {
    inst.src1_busy = false;
  }

  if (inst.src2_en) {
    inst.src2_preg = rename_table[inst.src2_areg];
    inst.src2_busy = busy_table[inst.src2_preg];
  } else {
    inst.src2_busy = false;
  }

  if (inst.dest_en)
    inst.old_dest_preg = rename_table[inst.dest_areg];

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

bool ISU::dispatch(Inst_Entry e) {

  for (auto &iq : iq_set) {
    if (iq.type == e.type) {
      if (iq.is_full())
        return false;

      if (e.type == LDU) {
        e.pre_store_num = iq_set[STU].entry.size();

        for (int i = 0; i < iq_set[STU].fu_num; i++) {
          if (!iq_set[STU].fu_set[i].ready) {
            e.pre_store_num++;
          }
        }
      }

      if (LOG)
        cout << "派遣指令" << hex << e.instruction << endl;

      e.pre_store_num = 0;
      rename(e);
      iq.enq(e);

      if (e.src1_en && e.src1_busy) {
        for (auto &iq : iq_set) {
          iq.add_depend(e.src1_preg);
        }
      }

      if (e.src2_en && e.src2_busy) {
        for (auto &iq : iq_set) {
          iq.add_depend(e.src2_preg);
        }
      }
    }
  }

  return true;
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
    cout << busy_table[i] << " ";
  }
  cout << endl;
}
