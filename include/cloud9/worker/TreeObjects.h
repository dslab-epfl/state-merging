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

namespace klee {
class KInstruction;
}

namespace cloud9 {

namespace worker {

#define TRACE_SIZE  64

/*
 *
 */
class SymbolicState {
	friend class JobManager;
	friend class StrategyPortfolio;
	friend class OracleStrategy;
private:
	klee::ExecutionState *kleeState;
	WorkerTree::NodePin nodePin;

	bool _active;

	std::vector<klee::KInstruction*> _instrProgress;
	unsigned int _instrPos;

	bool collectProgress;

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
		kleeState(state),
		nodePin(WORKER_LAYER_STATES),
		_active(false),
		_instrPos(0),
		collectProgress(false) {
      kleeState->setCloud9State(this);
	}

	virtual ~SymbolicState() {
		rebindToNode(NULL);
	}

	klee::ExecutionState *getKleeState() const { return kleeState; }

	WorkerTree::NodePin &getNode() { return nodePin; }
};

/*
 *
 */
class ExecutionJob {
	friend class JobManager;
	friend class StrategyPortfolio;
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
		if (nodePin) {
			(**nodePin).job = NULL;
			nodePin.reset();
		}
	}
public:
	ExecutionJob() : nodePin(WORKER_LAYER_JOBS), imported(false),
		exported(false), removing(false) {}

	ExecutionJob(WorkerTree::Node *node, bool _imported) :
		nodePin(WORKER_LAYER_JOBS), imported(_imported), exported(false),
		removing(false) {

		bindToNode(node);

		//CLOUD9_DEBUG("Created job at " << *node);
	}

	virtual ~ExecutionJob() {
		//CLOUD9_DEBUG("Destroyed job at " << nodePin);
		unbind();
	}

	WorkerTree::NodePin &getNode() { return nodePin; }

	bool isImported() const { return imported; }
	bool isExported() const { return exported; }
	bool isRemoving() const { return removing; }
};


std::ostream &operator<< (std::ostream &os, const SymbolicState &state);

}
}


#endif /* TREEOBJECTS_H_ */
