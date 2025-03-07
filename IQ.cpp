#include <IQ.h>
#include <ISU.h>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <vector>

Inst_Entry decode(uint32_t inst);
extern ISU isu;

void IQ::init(Fu_Type type, int entry_num, int fu_num) {
  this->type = type;
  this->fu_num = fu_num;
  this->max_entry_num = entry_num;
  fu_set = new FU[fu_num];

  for (int i = 0; i < fu_num; i++) {
    fu_set[i].type = type;
    fu_set[i].ready = true;
  }
}

void IQ::enq(Inst_Entry new_entry) { entry.push_back(new_entry); }

void IQ::add_depend(uint32_t preg) {
  for (auto &e : entry) {
    if (e.dest_en && e.dest_preg == preg) {
      e.dependency++;
#ifdef CONFIG_LONG_DEPEND
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
    fu->latency = 4;
  } else if (it->type == STU) {
    fu->latency = 2;
  }

  if (LOG) {
    cout << "发射指令" << hex << it->instruction << endl;
  }

  if (it->dest_en && it->type != LDU) {
    isu.awake(it->dest_preg);
  }
}

void IQ::deq() {
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
      issue(it, &fu_set[idle_fu_idx[issue_num++]]);
      issue_it.push_back(it);
    }

    it++;
  }

  for (auto it : issue_it) {
    entry.erase(it);
  }

#elif defined(CONFIG_MAX_DEPEND) || defined(CONFIG_LONG_DEPEND)
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
      issue(max_dep_it, &fu_set[idle_fu_idx[issue_num++]]);
      entry.erase(max_dep_it);
    } else {
      break;
    }
  }

#endif
}

void IQ::exec() {
  for (int i = 0; i < fu_num; i++) {
    if (fu_set[i].ready == false) {
      fu_set[i].latency--;

      // 执行完毕
      if (fu_set[i].latency == 0) {
        fu_set[i].ready = true;

        if (fu_set[i].inst.dest_en) {
          isu.idle_reg.push_back(fu_set[i].inst.old_dest_preg);
          if (fu_set[i].inst.type == LDU) {
            isu.awake(fu_set[i].inst.dest_preg);
          }
        }

        if (LOG)
          cout << "指令完毕" << hex << fu_set[i].inst.instruction << endl;
        if (type == STU) {
          isu.store_awake();
        }
      }
    }
  }
  /*for (auto i : idle_reg) {*/
  /*  cout << i << " ";*/
  /*}*/
  /*cout << endl;*/
}

bool IQ::is_full() { return entry.size() >= max_entry_num; }
bool IQ::is_empty() { return entry.size() == 0; }

void IQ::print() {
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
