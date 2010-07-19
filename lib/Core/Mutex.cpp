#include "klee/Mutex.h"

using namespace klee;
Mutex::Mutex(ref<Expr> mutex):
  address(mutex)
{
  thread = 0;
  taken = false;
  traceInfo.lastOp = 0;
  traceInfo.lastThread = 0;
  traceInfo.lclock = 0;
}

