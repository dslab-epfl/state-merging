/*
 * MultiProcess.h
 *
 *  Created on: Jul 22, 2010
 *      Author: stefan
 */

#ifndef MULTIPROCESS_H_
#define MULTIPROCESS_H_

#include <vector>

namespace klee {
class ExecutionState;
class Thread;

class Process {
  friend class Thread;
  friend class ExecutionState;
private:
  std::vector<unsigned int> forkPath; // 0 - parent, 1 - child

  ConstraintManager constraints;
  AddressSpace addressSpace;

  std::vector<Thread*> threads;

  /* Thread syncrhonization objects */
  std::map<ref<Expr>, Mutex> mutexes;
  std::map<ref<Expr>, CondVar> cond_vars;
  std::map< ref<Expr> , KFunction*> tls_keys;

  uint64_t tlsKeyCounter;

public:
  Process();

  uint64_t nextTLSKey() { return tlsKeyCounter++; }
};
}


#endif /* MULTIPROCESS_H_ */
