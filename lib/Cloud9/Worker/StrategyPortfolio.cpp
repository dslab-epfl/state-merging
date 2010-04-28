/*
 * StrategyPortfolio.cpp
 *
 *  Created on: Apr 28, 2010
 *      Author: stefan
 */

#include "cloud9/worker/StrategyPortfolio.h"
#include "cloud9/worker/TreeObjects.h"

namespace cloud9 {

namespace worker {

StrategyPortfolio::StrategyPortfolio(WorkerTree *_tree,
		std::map<strat_id_t, JobSelectionStrategy*> strategies) :
		tree(_tree), stratMap(strategies), position(0) {

	for (strat_map::iterator it = stratMap.begin(); it != stratMap.end(); it++) {
		stratVector.push_back(it->second);
	}
}

StrategyPortfolio::~StrategyPortfolio() {

}

bool StrategyPortfolio::isStateFree(SymbolicState *state) {
	WorkerTree::Node *node = state->getNode().get();
	return node->layerExists(WORKER_LAYER_JOBS) && !node->isLeaf(WORKER_LAYER_JOBS);
}

StrategyPortfolio::strat_id_t StrategyPortfolio::getStateStrategy(SymbolicState *state) {
	WorkerTree::Node *node = state->getNode().get();

	while ((**node).getJob() == NULL)
		node = node->getParent();

	assert(node != NULL);
	return (**node).getJob()->_strategy;
}

void StrategyPortfolio::onJobAdded(ExecutionJob *job) {
	strat_id_t id = job->_strategy;
	JobSelectionStrategy *strat = stratMap[id];

	assert(strat != NULL);

	strat->onJobAdded(job);
}

ExecutionJob* StrategyPortfolio::onNextJobSelection() {
	ExecutionJob *job = stratVector[position]->onNextJobSelection();

	position = (position + 1) % stratVector.size();

	return job;

}

void StrategyPortfolio::onRemovingJob(ExecutionJob *job) {
	strat_id_t id = job->_strategy;
	JobSelectionStrategy *strat = stratMap[id];

	assert(strat != NULL);

	strat->onRemovingJob(job);
}

void StrategyPortfolio::onRemovingJobs() {
	// Broadcast this to all strategies
	for (strat_map::iterator it = stratMap.begin(); it != stratMap.end(); it++) {
		JobSelectionStrategy *strat = it->second;

		strat->onRemovingJobs();
	}
}

void StrategyPortfolio::onStateActivated(SymbolicState *state) {
	bool free = isStateFree(state);

	if (free) {
		// Broadcast to everyone
		for (strat_map::iterator it = stratMap.begin(); it != stratMap.end(); it++) {
			JobSelectionStrategy *strat = it->second;

			strat->onStateActivated(state);
		}
	} else {
		strat_id_t id = getStateStrategy(state);
		JobSelectionStrategy *strat = stratMap[id];

		assert(strat != NULL);

		strat->onStateActivated(state);

	}

	state->_free = free;
}

void StrategyPortfolio::onStateUpdated(SymbolicState *state) {
	bool free = isStateFree(state);

	if (free) {
		assert(!state->_free && "State made free after being bound");

		// Broadcast to everyone
		for (strat_map::iterator it = stratMap.begin(); it != stratMap.end(); it++) {
			JobSelectionStrategy *strat = it->second;

			strat->onStateUpdated(state);
		}
	} else {
		if (state->_free) {
			// Now the state is bound to a single strategy, so deactivate it
			// from the other strategies
			strat_id_t id = getStateStrategy(state);

			for (strat_map::iterator it = stratMap.begin(); it != stratMap.end(); it++) {
				if (it->first != id) {
					JobSelectionStrategy *strat = it->second;
					strat->onStateDeactivated(state);
				}
			}

			JobSelectionStrategy *strat = stratMap[id];
			assert(strat != NULL);

			strat->onStateUpdated(state);
		}
	}

	state->_free = free;
}

void StrategyPortfolio::onStateDeactivated(SymbolicState *state) {
	bool free = isStateFree(state);

	if (free) {
		// Broadcast to everyone
		for (strat_map::iterator it = stratMap.begin(); it != stratMap.end(); it++) {
			JobSelectionStrategy *strat = it->second;

			strat->onStateDeactivated(state);
		}
	} else {
		strat_id_t id = getStateStrategy(state);
		JobSelectionStrategy *strat = stratMap[id];

		assert(strat != NULL);

		strat->onStateDeactivated(state);
	}
}

}

}
