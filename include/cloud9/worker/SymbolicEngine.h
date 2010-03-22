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
		virtual void onControlFlowEvent(klee::ExecutionState *state,
				ControlFlowEvent event) = 0;
		virtual void onDebugInfo(klee::ExecutionState *state,
				const std::string &message) = 0;

	};
private:
	typedef std::set<StateEventHandler*> handlers_t;
	handlers_t seHandlers;
protected:
	void fireStateBranched(klee::ExecutionState *state,
			klee::ExecutionState *parent, int index);
	void fireStateDestroy(klee::ExecutionState *state, bool &allow);
	void fireControlFlowEvent(klee::ExecutionState *state,
			ControlFlowEvent event);
	void fireDebugInfo(klee::ExecutionState *state, const std::string &message);

public:
	SymbolicEngine() {};
	virtual ~SymbolicEngine() {};

	virtual klee::ExecutionState *initRootState(llvm::Function *f, int argc,
			char **argv, char **envp) = 0;

	virtual void stepInState(klee::ExecutionState *state) = 0;

	virtual void destroyState(klee::ExecutionState *state) = 0;

	virtual void destroyStates() = 0;

	virtual klee::Searcher *initSearcher(klee::Searcher *base) = 0;

	void registerStateEventHandler(StateEventHandler *handler);
	void deregisterStateEventHandler(StateEventHandler *handler);
};

}

}

#endif /* SYMBOLICENGINE_H_ */
