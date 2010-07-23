/*
 * Threading.h
 *
 *  Created on: Jul 22, 2010
 *      Author: stefan
 */

#ifndef THREADING_H_
#define THREADING_H_

#include "klee/Expr.h"
#include "klee/Internal/Module/KInstIterator.h"
#include "cloud9/Logger.h"

#include <map>

namespace klee {

class KFunction;
class KInstruction;
class ExecutionState;
class Process;
class CallPathNode;
struct Cell;

typedef uint64_t thread_id_t;
typedef uint64_t process_id_t;

#define INVALID_THREAD_ID   ((uint64_t)(-1))

class Mutex {
  friend class Executor;
  friend class ExecutionState;
  friend class Thread;
private:
  ref<Expr> address;
  bool taken;
  int type;
  // the thread that holds this lock
  thread_id_t tid;
  std::vector<thread_id_t> waiting;

public:
  Mutex(ref<Expr> _address) :
    address(_address), taken(false), tid(0) {
  }
};



class CondVar {
  friend class Executor;
  friend class ExecutionState;
  friend class Thread;
public:
  CondVar(ref<Expr> _address) :
    address(_address) {
  }

private:
  ref<Expr> address;
  std::vector<thread_id_t> threads;
};

struct StackFrame {
  KInstIterator caller;
  KFunction *kf;
  CallPathNode *callPathNode;

  std::vector<const MemoryObject*> allocas;
  Cell *locals;

  /// Minimum distance to an uncovered instruction once the function
  /// returns. This is not a good place for this but is used to
  /// quickly compute the context sensitive minimum distance to an
  /// uncovered instruction. This value is updated by the StatsTracker
  /// periodically.
  unsigned minDistToUncoveredOnReturn;

  // For vararg functions: arguments not passed via parameter are
  // stored (packed tightly) in a local (alloca) memory object. This
  // is setup to match the way the front-end generates vaarg code (it
  // does not pass vaarg through as expected). VACopy is lowered inside
  // of intrinsic lowering.
  MemoryObject *varargs;

  StackFrame(KInstIterator caller, KFunction *kf);
  StackFrame(const StackFrame &s);

  StackFrame& operator=(const StackFrame &sf);
  ~StackFrame();
};


class Thread {
  friend class Executor;
  friend class ExecutionState;
  friend class Process;
private:
  static thread_id_t tidCounter;

  bool enabled;
  bool joinState;

  thread_id_t joining; // the thread we are joining
  thread_id_t tid;

  KInstIterator pc, prevPC;
  unsigned incomingBBIndex;

  std::vector<StackFrame> stack;

  std::map<ref<Expr> , ref<Expr> > tls;

  process_id_t pid;

public:
  Thread(KFunction *start_function);

  ~Thread();

  void pushFrame(KInstIterator caller, KFunction *kf);
  void popFrame();

};

}

#endif /* THREADING_H_ */
