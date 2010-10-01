/*
 * SymbolicEngine.cpp
 *
 *  Created on: Dec 25, 2009
 *      Author: stefan
 */

#include "cloud9/worker/SymbolicEngine.h"
#include "cloud9/instrum/InstrumentationManager.h"

#include <boost/bind.hpp>

namespace cloud9 {

namespace worker {

// TODO: Use boost functions to remove redundancy in the code

void SymbolicEngine::registerStateEventHandler(StateEventHandler *handler) {
	seHandlers.insert(handler);
}

void SymbolicEngine::deregisterStateEventHandler(StateEventHandler *handler) {
	seHandlers.erase(handler);
}

bool SymbolicEngine::fireStateBranching(klee::ExecutionState *state, klee::ForkTag forkTag) {
  if (seHandlers.size() == 0)
    return false;


  int result = true;

  for (handlers_t::iterator it = seHandlers.begin(); it != seHandlers.end(); it++) {
    StateEventHandler *h = *it;

    result = result && h->onStateBranching(state, forkTag);
  }

  return result;
}

void SymbolicEngine::fireStateBranched(klee::ExecutionState *state,
    klee::ExecutionState *parent, int index, klee::ForkTag forkTag) {

  fireHandler(boost::bind(&StateEventHandler::onStateBranched, _1, state, parent, index, forkTag));

  if (state) {
    cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalForkedStates);
    cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::CurrentStateCount);
  }
}

void SymbolicEngine::fireStateDestroy(klee::ExecutionState *state) {

  for (handlers_t::iterator it = seHandlers.begin(); it != seHandlers.end(); it++) {
    StateEventHandler *h = *it;

    h->onStateDestroy(state);
  }

  cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalFinishedStates);
  cloud9::instrum::theInstrManager.decStatistic(cloud9::instrum::CurrentStateCount);
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

void SymbolicEngine::fireEvent(klee::ExecutionState *state, unsigned int type,
    long int value) {
  fireHandler(boost::bind(&StateEventHandler::onEvent, _1, state, type, value));
}


}

}
