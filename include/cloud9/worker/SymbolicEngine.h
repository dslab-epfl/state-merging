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

class SymbolicEngine {
public:
	class StateEventHandler {
	public:
		StateEventHandler() {};
		virtual ~StateEventHandler() {};

	public:
		virtual void onStateBranched(klee::ExecutionState *state,
				klee::ExecutionState *parent, int index) = 0;
		virtual void onStateDestroy(klee::ExecutionState *state, bool &allow) = 0;

	};
private:
	typedef std::set<StateEventHandler*> handlers_t;
	handlers_t seHandlers;
protected:
	void fireStateBranched(klee::ExecutionState *state,
			klee::ExecutionState *parent, int index);
	void fireStateDestroy(klee::ExecutionState *state, bool &allow);
public:
	SymbolicEngine() {};
	virtual ~SymbolicEngine() {};

	virtual klee::ExecutionState *initRootState(llvm::Function *f, int argc,
			char **argv, char **envp) = 0;

	virtual void stepInState(klee::ExecutionState *state) = 0;

	virtual void destroyState(klee::ExecutionState *state) = 0;

	virtual void destroyStates() = 0;

	void registerStateEventHandler(StateEventHandler *handler);
	void deregisterStateEventHandler(StateEventHandler *handler);
};

}

}

#endif /* SYMBOLICENGINE_H_ */
