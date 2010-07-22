#include "klee/CondVar.h"

using namespace klee;

CondVar::CondVar(ref<Expr> condvar):
  address(condvar)
{ }

