#include <IQ.h>
#include <cstdint>
#include <vector>

class ISU {
  vector<IQ> iq_set = vector<IQ>(FU_TYPE_NUM);

public:
  void reset();
  bool is_empty();
  bool dispatch(Inst_Entry e);
  void deq();
  void print();
  void exec();
  void rename(Inst_Entry &inst);
  void awake(uint32_t dest_preg);
  void store_awake();

  vector<bool> busy_table;
  vector<uint32_t> idle_reg;
  vector<uint32_t> rename_table;
};
