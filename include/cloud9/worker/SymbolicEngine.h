/*
 * SymbolicEngine.h
 *
 *  Created on: Dec 23, 2009
 *      Author: stefan
 */

#ifndef SYMBOLICENGINE_H_
#define SYMBOLICENGINE_H_

#include <set>

namespace klee {
class ExecutionState;
}

namespace llvm {
class Function;
}

namespace cloud9 {

namespace worker {

class StateEventHandler;

class SymbolicEngine {
private:
	typedef std::set<StateEventHandler*> handlers_t;
	handlers_t seHandlers;
protected:
	void fireStateCreated(klee::ExecutionState *state);
	void fireStateDestroy(klee::ExecutionState *state, bool &allow);
public:
	SymbolicEngine() {};
	virtual ~SymbolicEngine() {};

	virtual klee::ExecutionState *initRootState(llvm::Function *f, int argc,
			char **argv, char **envp) = 0;

	virtual void destroyStates() = 0;

	void registerStateEventHandler(StateEventHandler *handler);
	void deregisterStateEventHandler(StateEventHandler *handler);
};

}

}

#endif /* SYMBOLICENGINE_H_ */
