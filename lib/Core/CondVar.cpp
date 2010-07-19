#include "klee/CondVar.h"

using namespace klee;

CondVar::CondVar(ref<Expr> condvar):
  address(condvar)
{
  traceInfo.lastOp = 0;
  traceInfo.lastThread = 0;
  traceInfo.lclock = 0;
}

