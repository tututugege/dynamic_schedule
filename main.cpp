#include "./IQ.h"
#include "./ISU.h"
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>

typedef struct {
  uint32_t pc;
  int num;
} Itrace_Node;

ISU isu;

int main() {

  // 加载指令trace
  ifstream trace("./dry_trace", ios::binary | ios::ate);
  streamsize size = trace.tellg();
  int trace_num = size / sizeof(Itrace_Node);
  Itrace_Node inst_trace[trace_num];
  trace.seekg(0, ios::beg);
  char *ptr = (char *)(&inst_trace);

  if (!trace.read(ptr, size)) {
    cout << "trace read error" << endl;
    exit(1);
  }

  // 加载程序
  ifstream image("./dry.bin", ios::binary | ios::ate);
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

  int i = 0;
  int time = 0;
  int commit_num = 0;

  for (int i = 0; i < trace_num; i++) {
    int block_len = inst_trace[i].num;
    int pc = inst_trace[i].pc;
    commit_num += block_len;

    int j = 0;
    while (j < block_len && time < MAX_SIM_TIME) {
      if (LOG) {
        cout << "********************" << endl;
        cout << "-- TIME: " << dec << time << " --" << endl;
      }
      isu.exec();
      isu.deq();
      do {
        bool ret = isu.dispatch(pc, inst[(pc - 0x80000000) >> 2]);

        if (ret == false) {
          break;
        } else {
          pc += 4;
          j++;
        }
      } while (j < block_len && (pc & 0b1100));
      time++;

      /*if (LOG)*/
      /*  isu.print();*/
      /*if (time % 10000 == 0) {*/
      /*  isu.print();*/
      /*}*/
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

    cout << "SUCCESS!!!" << endl;
    cout << "CYCLE: " << dec << time << endl;
    cout << "INST : " << dec << commit_num << endl;
    cout << "IPC  : " << (double)commit_num / time << endl;
  }

  return 0;
}
