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
#include "../../lib/Core/AddressSpace.h"
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
typedef uint64_t wlist_id_t;

typedef std::pair<thread_id_t, process_id_t> thread_uid_t;

struct LoopExecIndex {
  uint32_t loopID;
  uint32_t index;
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

  /// A stack of execution indexes. An item at index 0 corresponds to the
  /// non-loop function code, each next item corresponds to one loop level.
  /// This is updated by special function handlers for loop instrumentation.
  std::vector<LoopExecIndex> execIndexStack;

  StackFrame(KInstIterator caller, uint32_t _callerExecIndex, KFunction *kf);
  StackFrame(const StackFrame &s);

  StackFrame& operator=(const StackFrame &sf);
  ~StackFrame();
};


class Thread {
  friend class Executor;
  friend class ExecutionState;
  friend class Process;
private:

  KInstIterator pc, prevPC;
  unsigned incomingBBIndex;

  std::vector<StackFrame> stack;

  bool enabled;
  wlist_id_t waitingList;

  thread_uid_t tuid;

  uint32_t execIndex;
  uint32_t mergeIndex;

public:
  Thread(thread_id_t tid, process_id_t pid, KFunction *start_function);

  thread_id_t getTid() const { return tuid.first; }
  process_id_t getPid() const { return tuid.second; }
};

}

#endif /* THREADING_H_ */
