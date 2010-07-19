//===-- ScheduleTrace.h ----------------------------------------*- C++ -*-===//

#ifndef KLEE_SCHEDULETRACE_H
#define KLEE_SCHEDULETRACE_H

#include <stdint.h>
#include <string>

//using namespace std;

namespace klee {

class  TraceItem
{
public:
  TraceItem(uint64_t t1,
	    uint64_t op1, 
	    uint64_t t2, 
	    uint64_t op2);
  TraceItem(int tid);
  std::string _string() const;
private:
  uint64_t thread1;
  uint64_t op1;
  uint64_t thread2;
  uint64_t op2;
  bool isCreate;
};



struct SchedThreadTraceInfo
{
  uint64_t lclock; //Lamport clock value
  uint64_t op; // number of synch ops executed by the thread
  //also add the instruction 
};

struct SchedSyncTraceInfo
{
  unsigned int lclock; // Lamport clock value
  uint64_t lastThread;
  uint64_t lastOp;
};
}
#endif
