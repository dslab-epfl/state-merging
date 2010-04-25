/*
 * TreeObjects.h
 *
 *  Created on: Apr 24, 2010
 *      Author: stefan
 */

#ifndef TREEOBJECTS_H_
#define TREEOBJECTS_H_

#include "cloud9/worker/TreeNodeInfo.h"
#include "klee/ExecutionState.h"

namespace cloud9 {

namespace worker {

/*
 *
 */
class SymbolicState {
	friend class JobManager;
private:
	klee::ExecutionState *kleeState;
	WorkerTree::NodePin nodePin;

	void rebindToNode(WorkerTree::Node *node) {
		if (nodePin) {
			(**nodePin).symState = NULL;
		}

		nodePin = node->pin(WORKER_LAYER_STATES);
		(**node).symState = this;
	}
public:
	SymbolicState(klee::ExecutionState *state) :
		kleeState(state), nodePin(WORKER_LAYER_STATES) {
			kleeState->setCloud9State(this);
	}

	virtual ~SymbolicState() { }

	klee::ExecutionState *getKleeState() const { return kleeState; }

	WorkerTree::NodePin &getNode() { return nodePin; }
};

/*
 *
 */
class ExecutionJob {
	friend class JobManager;
private:
	WorkerTree::NodePin nodePin;

	bool imported;
	bool exported;
	bool removing;

	void bindToNode(WorkerTree::Node *node) {
		assert(!nodePin);

		nodePin = node->pin(WORKER_LAYER_JOBS);
		(**node).job = this;
	}
public:
	ExecutionJob() : nodePin(WORKER_LAYER_JOBS), imported(false), exported(false), removing(false) {}
	virtual ~ExecutionJob() {}

	WorkerTree::NodePin &getNode() { return nodePin; }

	bool isImported() const { return imported; }
	bool isExported() const { return exported; }
	bool isRemoving() const { return removing; }
};

}
}


#endif /* TREEOBJECTS_H_ */
