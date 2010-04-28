/*
 * StrategyPortfolio.h
 *
 *  Created on: Apr 28, 2010
 *      Author: stefan
 */

#ifndef STRATEGYPORTFOLIO_H_
#define STRATEGYPORTFOLIO_H_

#include "cloud9/worker/CoreStrategies.h"
#include "cloud9/worker/TreeNodeInfo.h"

#include <map>

namespace cloud9 {

namespace worker {

class StrategyPortfolio: public JobSelectionStrategy {
public:
	typedef unsigned int strat_id_t;
private:
	WorkerTree *tree;

	typedef std::map<strat_id_t, JobSelectionStrategy*> strat_map;
	typedef std::vector<JobSelectionStrategy*> strat_vector;

	strat_map stratMap;
	strat_vector stratVector;

	unsigned int position;

	static inline bool isStateFree(SymbolicState *state);
	static strat_id_t getStateStrategy(SymbolicState *state);
public:
	StrategyPortfolio(WorkerTree *_tree,
			std::map<strat_id_t, JobSelectionStrategy*> strategies);

	virtual ~StrategyPortfolio();

	virtual void onJobAdded(ExecutionJob *job);
	virtual ExecutionJob* onNextJobSelection();
	virtual void onRemovingJob(ExecutionJob *job);
	virtual void onRemovingJobs();

	virtual void onStateActivated(SymbolicState *state);
	virtual void onStateUpdated(SymbolicState *state);
	virtual void onStateDeactivated(SymbolicState *state);
};

}

}

#endif /* STRATEGYPORTFOLIO_H_ */
