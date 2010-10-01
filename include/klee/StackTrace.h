/*
 * StackTrace.h
 *
 *  Created on: Oct 1, 2010
 *      Author: stefan
 */

#ifndef STACKTRACE_H_
#define STACKTRACE_H_

#include "klee/Expr.h"

#include <vector>
#include <iostream>

namespace klee {

class KFunction;
class KInstruction;

class StackTrace {
  friend class ExecutionState;
private:
  typedef std::pair<KFunction*, const KInstruction*> position_t;

  typedef std::pair<position_t, std::vector<ref<Expr> > > frame_t;

  typedef std::vector<frame_t> stack_t;

  stack_t contents;

  StackTrace() { } // Cannot construct directly
public:
  void dump(std::ostream &out) const;
};

}


#endif /* STACKTRACE_H_ */
