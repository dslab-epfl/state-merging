/*
 * MultiProcess.h
 *
 *  Created on: Jul 22, 2010
 *      Author: stefan
 */

#ifndef MULTIPROCESS_H_
#define MULTIPROCESS_H_

#include "klee/Constraints.h"
#include "../../lib/Core/AddressSpace.h"
#include "klee/Threading.h"

#include <vector>
#include <set>
#include <cstdint>
#include <map>

namespace klee {
class ExecutionState;
class Thread;

typedef uint64_t process_id_t;
typedef uint64_t thread_id_t;
typedef uint64_t wlist_id_t;

#define INVALID_PROCESS_ID ((uint64_t)(-1))

class Process {
  friend class Thread;
  friend class ExecutionState;
  friend class Executor;
private:

  static process_id_t pidCounter;

  std::vector<unsigned int> forkPath; // 0 - parent, 1 - child

  /* Thread syncrhonization objects */
  std::map<ref<Expr>, Mutex> mutexes;
  std::map<ref<Expr>, CondVar> cond_vars;

public:
  Process();

  process_id_t pid;
  process_id_t ppid;

  std::map<process_id_t, wlist_id_t> children;
  wlist_id_t anyChild;

  std::map<thread_id_t, wlist_id_t> threads;

  AddressSpace addressSpace;
};
}


#endif /* MULTIPROCESS_H_ */
