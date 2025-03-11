#include "ISU.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

Inst_Entry decode(uint32_t inst);

void ISU::init() {
  iq_set[0].init(ALU, 8, 4);
  iq_set[1].init(LDU, 4, 1);
  iq_set[2].init(STU, 4, 1);
  iq_set[3].init(BRU, 4, 1);

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

void ISU::reset() {
  cache.reset();
  bpu.reset();
  sim_end = false;
  stall = false;
  commit_num = 0;

  branch_num = 0;
  mispred_num = 0;

  for (int i = 0; i < MAX_PREG; i++) {
    busy_table[i] = false;
  }

  idle_reg.clear();
  for (int i = 32; i < MAX_PREG; i++) {
    idle_reg.push_back(i);
  }

  for (int i = 0; i < 32; i++) {
    rename_table[i] = i;
  }

  iq_set[0].reset();
  iq_set[1].reset();
  iq_set[2].reset();
  iq_set[3].reset();
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

#ifdef CONFIG_BR_DEPEND
      if (e.type == BRU && e.mispred) {
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
#else

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

#endif
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
    iq.exec(this);
}

void ISU::deq() {
  for (auto &iq : iq_set)
    iq.deq(this);
}

bool ISU::deq(pair<uint32_t, uint32_t> issue_inst) {
  int idle_fu = -1;
  for (int i = 0; i < iq_set[issue_inst.first].fu_num; i++) {
    if (iq_set[issue_inst.first].fu_set[i].ready) {
      idle_fu = i;
    }
  }

  if (idle_fu == -1) {
    assert(0);
  }

  int issue_num = 0;
  list<Inst_Entry>::iterator it = iq_set[issue_inst.first].entry.begin();
  advance(it, issue_inst.second);
  if ((!it->src1_en || !it->src1_busy) && (!it->src2_en || !it->src2_busy) &&
      (it->type != LDU || it->pre_store_num == 0)) {
    iq_set[issue_inst.first].issue(
        it, &iq_set[issue_inst.first].fu_set[idle_fu], this);

    // 不满足发射条件
  } else {
    assert(0);
  }

  iq_set[issue_inst.first].entry.erase(it);
}

void ISU::deq(vector<vector<uint32_t>> issue_idx) {
  for (int i = 0; i < issue_idx.size(); i++) {
    // 找到对应iq空闲的FU
    vector<int> idle_fu_idx;
    int idle_fu_num = 0;

    for (int j = 0; j < iq_set[i].fu_num; j++) {

      if (iq_set[i].fu_set[j].ready) {
        idle_fu_idx.push_back(j);
        idle_fu_num++;
      }
    }

    // 发射的指令数大于执行单元
    if (idle_fu_num < issue_idx[i].size()) {
      cout << i << endl;
      cout << idle_fu_num << endl;
      cout << issue_idx[i].size() << endl;
      assert(0);
    }

    int issue_num = 0;
    vector<list<Inst_Entry>::iterator> issue_it;

    for (auto idx : issue_idx[i]) {
      list<Inst_Entry>::iterator it = iq_set[i].entry.begin();
      advance(it, idx);
      if ((!it->src1_en || !it->src1_busy) &&
          (!it->src2_en || !it->src2_busy) &&
          (it->type != LDU || it->pre_store_num == 0)) {
        iq_set[i].issue(it, &iq_set[i].fu_set[idle_fu_idx[issue_num++]], this);
        issue_it.push_back(it);

        // 不满足发射条件
      } else {
        assert(0);
      }
    }

    for (auto it : issue_it) {
      iq_set[i].entry.erase(it);
    }
  }
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
