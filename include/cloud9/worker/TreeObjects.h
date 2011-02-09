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
#include "klee/ForkTag.h"

#include <ostream>

namespace klee {
class KInstruction;
}

namespace cloud9 {

namespace worker {

/*
 *
 */
class SymbolicState {
	friend class JobManager;
	friend class OracleStrategy;
private:
	klee::ExecutionState *kleeState;
	WorkerTree::NodePin nodePin;

	bool _active;

	std::vector<klee::KInstruction*> _instrProgress;
	unsigned int _instrPos;
	unsigned long _instrSinceFork;

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
		_instrSinceFork(0),
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
private:
	WorkerTree::NodePin nodePin;

	bool imported;
	bool exported;
	bool removing;

	klee::ForkTag forkTag;

    void rebindToNode(WorkerTree::Node *node) {
        if (nodePin) {
            (**nodePin).job = NULL;
        }

        if (node) {
            nodePin = node->pin(WORKER_LAYER_JOBS);
            (**node).job = this;
        } else {
            nodePin.reset();
        }
    }
public:
	ExecutionJob() : nodePin(WORKER_LAYER_JOBS), imported(false),
		exported(false), removing(false), forkTag(klee::KLEE_FORK_DEFAULT) {}

	ExecutionJob(WorkerTree::Node *node, bool _imported) :
		nodePin(WORKER_LAYER_JOBS), imported(_imported), exported(false),
		removing(false), forkTag(klee::KLEE_FORK_DEFAULT) {

		rebindToNode(node);

		//CLOUD9_DEBUG("Created job at " << *node);
	}

	virtual ~ExecutionJob() {
		//CLOUD9_DEBUG("Destroyed job at " << nodePin);
		rebindToNode(NULL);
	}

	WorkerTree::NodePin &getNode() { return nodePin; }

	klee::ForkTag getForkTag() const { return forkTag; }

	bool isImported() const { return imported; }
	bool isExported() const { return exported; }
	bool isRemoving() const { return removing; }
};


std::ostream &operator<< (std::ostream &os, const SymbolicState &state);

}
}


#endif /* TREEOBJECTS_H_ */
