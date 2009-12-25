/*
 * SymbolicEngine.cpp
 *
 *  Created on: Dec 25, 2009
 *      Author: stefan
 */

#include "cloud9/worker/SymbolicEngine.h"

#include "cloud9/worker/StateEventHandler.h"

namespace cloud9 {

namespace worker {

void SymbolicEngine::registerStateEventHandler(StateEventHandler *handler) {
	seHandlers.insert(handler);
}

void SymbolicEngine::deregisterStateEventHandler(StateEventHandler *handler) {
	seHandlers.erase(handler);
}

void SymbolicEngine::fireStateCreated(klee::ExecutionState *state) {
	for (handlers_t::iterator it = seHandlers.begin(); it != seHandlers.end(); it++) {
		StateEventHandler *h = *it;

		h->onStateCreated(state);
	}
}

void SymbolicEngine::fireStateDestroy(klee::ExecutionState *state, bool &allow) {
	allow = true;

	for (handlers_t::iterator it = seHandlers.begin(); it != seHandlers.end(); it++) {
		StateEventHandler *h = *it;
		bool crtAllow = true;

		h->onStateDestroy(state, crtAllow);

		allow = allow && crtAllow;
	}
}

}

}
