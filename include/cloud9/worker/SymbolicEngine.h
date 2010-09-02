/*
 * SymbolicEngine.h
 *
 *  Created on: Dec 23, 2009
 *      Author: stefan
 */

#ifndef SYMBOLICENGINE_H_
#define SYMBOLICENGINE_H_

#include <set>
#include <string>

namespace klee {
class ExecutionState;
class Searcher;
class KModule;
}

namespace llvm {
class Function;
}

namespace cloud9 {

namespace worker {

enum ControlFlowEvent {
	STEP,
	BRANCH_FALSE,
	BRANCH_TRUE,
	CALL,
	RETURN
};

class StateEventHandler {
public:
	StateEventHandler() {};
	virtual ~StateEventHandler() {};

public:
	virtual bool onStateBranching(klee::ExecutionState *state, int reason) = 0;
	virtual void onStateBranched(klee::ExecutionState *state,
			klee::ExecutionState *parent, int index, int reason) = 0;
	virtual void onStateDestroy(klee::ExecutionState *state) = 0;
	virtual void onControlFlowEvent(klee::ExecutionState *state,
			ControlFlowEvent event) = 0;
	virtual void onDebugInfo(klee::ExecutionState *state,
			const std::string &message) = 0;
	virtual void onBreakpoint(klee::ExecutionState *state,
	    unsigned int id) = 0;
	virtual void onOutOfResources(klee::ExecutionState *destroyedState) = 0;

};

class SymbolicEngine {
private:
	typedef std::set<StateEventHandler*> handlers_t;
	handlers_t seHandlers;

	template <class Handler>
	void fireHandler(Handler handler) {
	  for (handlers_t::iterator it = seHandlers.begin(); it != seHandlers.end(); it++) {
        StateEventHandler *h = *it;

        handler(h);
      }
	}
protected:
	bool fireStateBranching(klee::ExecutionState *state, int reason);
	void fireStateBranched(klee::ExecutionState *state,
			klee::ExecutionState *parent, int index, int reason);
	void fireStateDestroy(klee::ExecutionState *state);
	void fireControlFlowEvent(klee::ExecutionState *state,
			ControlFlowEvent event);
	void fireDebugInfo(klee::ExecutionState *state, const std::string &message);
	void fireOutOfResources(klee::ExecutionState *destroyedState);
	void fireBreakpoint(klee::ExecutionState *state, unsigned int id);
public:
	SymbolicEngine() {};
	virtual ~SymbolicEngine() {};

	virtual klee::ExecutionState *createRootState(llvm::Function *f) = 0;
	virtual void initRootState(klee::ExecutionState *state, int argc,
			char **argv, char **envp) = 0;

	virtual void stepInState(klee::ExecutionState *state) = 0;

	virtual void destroyState(klee::ExecutionState *state) = 0;

	virtual void destroyStates() = 0;

	virtual klee::Searcher *initSearcher(klee::Searcher *base) = 0;

	virtual klee::KModule *getModule() = 0;

	void registerStateEventHandler(StateEventHandler *handler);
	void deregisterStateEventHandler(StateEventHandler *handler);
};

}

}

#endif /* SYMBOLICENGINE_H_ */
