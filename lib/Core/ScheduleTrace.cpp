#include "klee/ScheduleTrace.h"
#include <sstream>

using namespace klee;

TraceItem::TraceItem(uint64_t t1,
		     uint64_t op1, 
		     uint64_t t2, 
		     uint64_t op2):
  thread1(t1),
  op1(op1), 
  thread2(t2), 
  op2(op2), 
  isCreate( false)
{
}

TraceItem::TraceItem(int tid):
  thread1(tid),
  op1(0), 
  thread2(0), 
  op2(0),
  isCreate(true)
{
}

std::string TraceItem::_string() const
{
  std::stringstream str;
  if(!isCreate)
    str << thread1 << " " << 
      op1 << " " << 
      thread2 << " " << 
      op2;
  else 
    str << thread1 << " " << 
      0 << " " << 
      -1 << " " << 
      0;
  return str.str();
}

