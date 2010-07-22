//===-- CondVar.h ----------------------------------------*- C++ -*-===//
#ifndef KLEE_CONDVAR_H
#define KLEE_CONDVAR_H

#include "klee/Expr.h"
#include <vector>

namespace klee {

class CondVar
{
  friend class Executor;
  friend class ExecutionState;
  friend class Thread;
public:
  CondVar(ref<Expr> address);
  CondVar() { }
private:
  ref<Expr> address;
  std::vector<uint64_t> threads;
};
}
#endif
