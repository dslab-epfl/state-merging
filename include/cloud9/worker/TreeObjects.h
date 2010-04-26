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

#include <ostream>

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

	bool _active;

	void rebindToNode(WorkerTree::Node *node) {
		if (nodePin) {
			(**nodePin).symState = NULL;
		}

		if (node) {
			nodePin = node->pin(WORKER_LAYER_STATES);
			(**node).symState = this;
		} else {
			nodePin.reset();
		}
	}
public:
	SymbolicState(klee::ExecutionState *state) :
		kleeState(state), nodePin(WORKER_LAYER_STATES), _active(false) {
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
		assert(!nodePin && node);

		nodePin = node->pin(WORKER_LAYER_JOBS);
		(**node).job = this;
	}

	void unbind() {
		assert(nodePin);

		(**nodePin).job = NULL;
		nodePin.reset();
	}
public:
	ExecutionJob() : nodePin(WORKER_LAYER_JOBS), imported(false),
		exported(false), removing(false) {}

	ExecutionJob(WorkerTree::Node *node, bool _imported) :
		nodePin(WORKER_LAYER_JOBS), imported(_imported), exported(false),
		removing(false) {

		bindToNode(node);
	}

	virtual ~ExecutionJob() {}

	WorkerTree::NodePin &getNode() { return nodePin; }

	bool isImported() const { return imported; }
	bool isExported() const { return exported; }
	bool isRemoving() const { return removing; }
};


std::ostream &operator<< (std::ostream &os, const SymbolicState &state);

}
}


#endif /* TREEOBJECTS_H_ */
