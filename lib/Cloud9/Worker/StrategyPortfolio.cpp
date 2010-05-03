/*
 * StrategyPortfolio.cpp
 *
 *  Created on: Apr 28, 2010
 *      Author: stefan
 */

#include "cloud9/worker/StrategyPortfolio.h"
#include "cloud9/worker/TreeObjects.h"
#include "cloud9/worker/JobManager.h"

#include <boost/bind.hpp>

namespace cloud9 {

namespace worker {

StrategyPortfolio::StrategyPortfolio(JobManager *_manager, std::map<strat_id_t,
    JobSelectionStrategy*> &strategies) :
  manager(_manager), position(0) {

  tree = manager->getTree();

  for (std::map<strat_id_t, JobSelectionStrategy*>::iterator it =
      strategies.begin(); it != strategies.end(); it++) {
    stratMap[it->first].strategy = it->second;
    stratMap[it->first].allocation = 0;
    stratMap[it->first].performance = 0;

    stratVector.push_back(it->second);
    stratIdVector.push_back(it->first);
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

bool StrategyPortfolio::isValidJob(strat_id_t strat, WorkerTree::Node *jobNode) {
	if (jobNode == manager->getCurrentNode())
		return false;

	assert((**jobNode).getJob() != NULL);
	return (**jobNode).getJob()->_strategy == strat;
}

void StrategyPortfolio::onJobAdded(ExecutionJob *job) {
	strat_id_t id = job->_strategy;
	JobSelectionStrategy *strat = stratMap[id].strategy;

	assert(strat != NULL);

	strat->onJobAdded(job);

	stratMap[id].allocation++;
}

ExecutionJob* StrategyPortfolio::onNextJobSelection() {
  unsigned int crtPos = position;
  ExecutionJob *job;

  do {
    job = stratVector[crtPos]->onNextJobSelection();
    crtPos = (crtPos + 1) % stratVector.size();

    if (job != NULL)
      break;
  } while (crtPos != position);

  position = crtPos;

  return job;

}

void StrategyPortfolio::onRemovingJob(ExecutionJob *job) {
	strat_id_t id = job->_strategy;
	JobSelectionStrategy *strat = stratMap[id].strategy;

	assert(strat != NULL);

	strat->onRemovingJob(job);

	stratMap[id].allocation--;
}

void StrategyPortfolio::onStateActivated(SymbolicState *state) {
	bool free = isStateFree(state);
	strat_id_t id = 0;

	if (free) {
		// Broadcast to everyone
		for (strat_map::iterator it = stratMap.begin(); it != stratMap.end(); it++) {
			JobSelectionStrategy *strat = it->second.strategy;

			strat->onStateActivated(state);
		}
	} else {
		id = getStateStrategy(state);
		JobSelectionStrategy *strat = stratMap[id].strategy;

		assert(strat != NULL);

		strat->onStateActivated(state);

	}

	state->_free = free;
	state->_strategy = id;
}

void StrategyPortfolio::onStateUpdated(SymbolicState *state) {
	bool free = isStateFree(state);
	strat_id_t id = 0;

	if (free) {
		assert(state->_free && "State made free after being bound");

		// Broadcast to everyone
		for (strat_map::iterator it = stratMap.begin(); it != stratMap.end(); it++) {
			JobSelectionStrategy *strat = it->second.strategy;

			strat->onStateUpdated(state);
		}
	} else {
	    id = getStateStrategy(state);

		if (state->_free) {
			// Now the state is bound to a single strategy, so deactivate it
			// from the other strategies

			for (strat_map::iterator it = stratMap.begin(); it != stratMap.end(); it++) {
				if (it->first != id) {
					JobSelectionStrategy *strat = it->second.strategy;
					strat->onStateDeactivated(state);
				}
			}

			JobSelectionStrategy *strat = stratMap[id].strategy;
			assert(strat != NULL);

			strat->onStateUpdated(state);
		}
	}

	state->_free = free;
	state->_strategy = id;
}

void StrategyPortfolio::onStateDeactivated(SymbolicState *state) {
	if (state->_free) {
		// Broadcast to everyone
		for (strat_map::iterator it = stratMap.begin(); it != stratMap.end(); it++) {
			JobSelectionStrategy *strat = it->second.strategy;

			strat->onStateDeactivated(state);
		}
	} else {
		strat_id_t id = state->_strategy;

		JobSelectionStrategy *strat = stratMap[id].strategy;

		assert(strat != NULL);

		strat->onStateDeactivated(state);
	}
}

void StrategyPortfolio::reInvestJobs(strat_id_t newStrat, strat_id_t oldStrat, unsigned int maxCount) {
	manager->lockJobs();

	// Select the jobs containing the old strategy
	std::vector<WorkerTree::Node*> nodes;

	tree->getLeaves(WORKER_LAYER_JOBS, tree->getRoot(),
			boost::bind(&StrategyPortfolio::isValidJob, this, oldStrat, _1),
			maxCount, nodes);

	std::vector<ExecutionJob*> jobs;

	for (std::vector<WorkerTree::Node*>::iterator it = nodes.begin();
			it != nodes.end(); it++) {
		WorkerTree::Node *node = *it;

		jobs.push_back((**node).getJob());
	}

	reInvestJobs(newStrat, jobs);

	manager->unlockJobs();
}

void StrategyPortfolio::reInvestJobs(strat_id_t newStrat, std::vector<ExecutionJob*> &jobs) {
	for (std::vector<ExecutionJob*>::iterator it = jobs.begin();
			it != jobs.end(); it++) {

		ExecutionJob *job = *it;
		if (job->_strategy == newStrat) {
			// Nothing to do here, move on
			continue;
		}

		WorkerTree::Node *node = job->getNode().get();

		if ((**node).getSymbolicState() != NULL) {
			SymbolicState *state = (**node).getSymbolicState();

			// We also need to move the state from one strategy to another
			stratMap[job->_strategy].strategy->onStateDeactivated(state);
			stratMap[newStrat].strategy->onStateActivated(state);
		}

		stratMap[job->_strategy].strategy->onRemovingJob(job);
		stratMap[newStrat].strategy->onJobAdded(job);

		stratMap[job->_strategy].allocation--;
		stratMap[newStrat].allocation++;

		job->_strategy = newStrat;
	}
}

}

}
