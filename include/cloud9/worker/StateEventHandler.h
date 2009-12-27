/*
 * StateEventHandler.h
 *
 *  Created on: Dec 24, 2009
 *      Author: stefan
 */

#ifndef STATEEVENTHANDLER_H_
#define STATEEVENTHANDLER_H_

namespace klee{
class ExecutionState;
}

namespace cloud9 {

namespace worker {

class SymbolicEngine;

class StateEventHandler {
	friend class SymbolicEngine;
public:
	StateEventHandler() {};
	virtual ~StateEventHandler() {};

protected:
	virtual void onStateBranched(klee::ExecutionState *state,
			klee::ExecutionState *parent, int index) = 0;
	virtual void onStateDestroy(klee::ExecutionState *state, bool &allow) = 0;

};

}
}

#endif /* STATEEVENTHANDLER_H_ */
