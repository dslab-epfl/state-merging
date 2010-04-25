/*
 * SymbolicEngine.cpp
 *
 *  Created on: Dec 25, 2009
 *      Author: stefan
 */

#include "cloud9/worker/SymbolicEngine.h"

namespace cloud9 {

namespace worker {

// TODO: Use boost functions to remove redundancy in the code

void SymbolicEngine::registerStateEventHandler(StateEventHandler *handler) {
	seHandlers.insert(handler);
}

void SymbolicEngine::deregisterStateEventHandler(StateEventHandler *handler) {
	seHandlers.erase(handler);
}

void SymbolicEngine::fireStateBranched(klee::ExecutionState *state,
		klee::ExecutionState *parent, int index) {

	for (handlers_t::iterator it = seHandlers.begin(); it != seHandlers.end(); it++) {
		StateEventHandler *h = *it;

		h->onStateBranched(state, parent, index);
	}
}

void SymbolicEngine::fireStateDestroy(klee::ExecutionState *state) {

	for (handlers_t::iterator it = seHandlers.begin(); it != seHandlers.end(); it++) {
		StateEventHandler *h = *it;

		h->onStateDestroy(state);
	}
}

void SymbolicEngine::fireControlFlowEvent(klee::ExecutionState *state,
			ControlFlowEvent event) {
	for (handlers_t::iterator it = seHandlers.begin(); it != seHandlers.end(); it++) {
		StateEventHandler *h = *it;

		h->onControlFlowEvent(state, event);
	}
}

void SymbolicEngine::fireDebugInfo(klee::ExecutionState *state,
		const std::string &message) {
	for (handlers_t::iterator it = seHandlers.begin(); it != seHandlers.end(); it++) {
		StateEventHandler *h = *it;

		h->onDebugInfo(state, message);
	}
}

void SymbolicEngine::fireOutOfResources(klee::ExecutionState *destroyedState) {
	for (handlers_t::iterator it = seHandlers.begin(); it != seHandlers.end(); it++) {
		StateEventHandler *h = *it;

		h->onOutOfResources(destroyedState);
	}
}


}

}
