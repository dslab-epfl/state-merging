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
class StackFrame;

class Mutex {
  friend class Executor;
  friend class ExecutionState;
  friend class Thread;
private:
  ref<Expr> address;
  bool taken;
  int type;
  // the thread that holds this lock
  uint64_t thread;
  std::vector<uint64_t> waiting;

public:
  Mutex(ref<Expr> _address) :
    address(_address), taken(false), thread(0) {
  }
  //Mutex() { }

};

class CondVar {
  friend class Executor;
  friend class ExecutionState;
  friend class Thread;
public:
  CondVar(ref<Expr> _address) :
    address(_address) {
  }
  //CondVar() { }
private:
  ref<Expr> address;
  std::vector<uint64_t> threads;
};

class Thread {
  friend class Executor;
  friend class ExecutionState;
  friend class Mutex;
  friend class CondVar;
private:
  bool enabled;
  bool joinState;
  // the thread we are joining
  uint64_t joining;
  ref<Expr> thread_ptr; //address of the thread variable
  uint64_t tid;
  KInstIterator pc, prevPC;

  std::vector<StackFrame> stack;

  std::map<ref<Expr> , ref<Expr> > tls;
  std::string _file; //XXX hack to store the top frame info for bktrace
  unsigned _line;

  void pushFrame(KInstIterator caller, KFunction *kf);
  bool isInJoin() {
    return joinState;
  }

public:
  Thread(ref<Expr> _address, KFunction *start_function);

  static uint64_t tids;

  uint64_t getTID() {
    return tid;
  }
  KInstIterator getPC() {
    return pc;
  }

};

}

#endif /* THREADING_H_ */
