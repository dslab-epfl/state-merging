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

class JobManager;

class StrategyPortfolio: public JobSelectionStrategy {
public:
	typedef unsigned int strat_id_t;
	typedef std::vector<strat_id_t> strat_id_vector;
private:
	struct strat_info {
		JobSelectionStrategy *strategy;
		unsigned int allocation;
		unsigned int performance;
	};

	JobManager *manager;
	WorkerTree *tree;

	typedef std::map<strat_id_t, strat_info> strat_map;

	typedef std::vector<JobSelectionStrategy*> strat_vector;

	strat_map stratMap;

	strat_vector stratVector;
	strat_id_vector stratIdVector;

	unsigned int position;

	static inline bool isStateFree(SymbolicState *state);
	static strat_id_t getStateStrategy(SymbolicState *state);

	bool isValidJob(strat_id_t strat, WorkerTree::Node *jobNode);

	void reInvestJobs(strat_id_t newStrat, std::vector<ExecutionJob*> &jobs);
public:
	StrategyPortfolio(JobManager *_manager,
			std::map<strat_id_t, JobSelectionStrategy*> &strategies);

	virtual ~StrategyPortfolio();

	const strat_id_vector &getStrategies() const { return stratIdVector; }
	JobSelectionStrategy* getStrategy(strat_id_t id) { return stratMap[id].strategy; }
	unsigned int getStrategyAllocation(strat_id_t id) { return stratMap[id].allocation; }
	unsigned int getStrategyPerformance(strat_id_t id) { return stratMap[id].performance; }

	virtual void onJobAdded(ExecutionJob *job);
	virtual ExecutionJob* onNextJobSelection();
	virtual void onRemovingJob(ExecutionJob *job);

	virtual void onStateActivated(SymbolicState *state);
	virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode);
	virtual void onStateDeactivated(SymbolicState *state);

	void reInvestJobs(strat_id_t newStrat, strat_id_t oldStrat, unsigned int maxCount);
};

}

}

#endif /* STRATEGYPORTFOLIO_H_ */
