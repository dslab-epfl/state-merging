/*
 * LoadBalancer.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef LOADBALANCER_H_
#define LOADBALANCER_H_

#include "cloud9/lb/TreeNodeInfo.h"
#include "cloud9/lb/Worker.h"
#include "cloud9/lb/StrategyStatistic.h"

#include <set>
#include <map>

namespace cloud9 {

class ExecutionPath;

namespace lb {

class TransferRequest {
public:
	worker_id_t fromID;
	worker_id_t toID;

	ExecutionPathSetPin paths;
	std::vector<int> counts;
public:
	TransferRequest(worker_id_t from, worker_id_t to) : fromID(from), toID(to) { }
};

class InvestmentRequest {
public:
	strat_id_t fromID;
	strat_id_t toID;

	unsigned int count;
public:
	InvestmentRequest(strat_id_t from, strat_id_t to, unsigned int _count) :
		fromID(from), toID(to), count(_count) { }
};

// TODO: Refactor this into a more generic class
class LoadBalancer {
private:
	LBTree *tree;

	std::string programName;
	unsigned statIDCount;
	unsigned programCRC;

	worker_id_t nextWorkerID;

	unsigned balanceRate;
	unsigned rounds;

	unsigned activeCount;

	std::map<worker_id_t, Worker*> workers;
	std::set<worker_id_t> reqDetails;
	std::map<worker_id_t, TransferRequest*> reqTransfer;
	std::map<worker_id_t, std::vector<InvestmentRequest*> > reqsInvest;

	std::set<worker_id_t> reports;

	// TODO: Restructure this to a more intuitive representation with
	// global coverage + coverage deltas
	std::vector<uint64_t> globalCoverageData;
	std::vector<char> globalCoverageUpdates;

	TransferRequest *computeTransfer(worker_id_t fromID, worker_id_t toID, unsigned count);

	void computeGlobalPortfolioStats(strat_stat_map &portfolioStats);

public:
	LoadBalancer(int balanceRate);
	virtual ~LoadBalancer();

	worker_id_t registerWorker(const std::string &address, int port, bool wantsUpdates);
	void deregisterWorker(worker_id_t id);

	void registerProgramParams(const std::string &programName, unsigned crc, unsigned statIDCount);
	void checkProgramParams(const std::string &programName, unsigned crc, unsigned statIDCount);

	void analyzeBalance();

	LBTree *getTree() const { return tree; }

	unsigned getActiveCount() const { return activeCount; }

	void updateWorkerStatNodes(worker_id_t id, std::vector<LBTree::Node*> &newNodes);
	void updateWorkerStats(worker_id_t id, std::vector<int> &stats);

	void updateStrategyPortfolioStats(worker_id_t id, strat_stat_map &stats);

	void updateCoverageData(worker_id_t id, const cov_update_t &data);

	Worker* getWorker(worker_id_t id) {
		std::map<worker_id_t, Worker*>::iterator it = workers.find(id);
		if (it == workers.end())
			return NULL;
		else
			return (*it).second;
	}

	int getWorkerCount() { return workers.size(); }

	bool requestAndResetDetails(worker_id_t id) {
		bool result = reqDetails.count(id) > 0;

		if (result) reqDetails.erase(id);

		return result;
	}

	void getAndResetCoverageUpdates(worker_id_t id, cov_update_t &data);

	TransferRequest *requestAndResetTransfer(worker_id_t id) {
		if (reqTransfer.count(id) > 0) {
			TransferRequest *result = reqTransfer[id];
			if (result->fromID == id) {
				reqTransfer.erase(result->fromID);
				reqTransfer.erase(result->toID);
				return result;
			}
		}

		return NULL;
	}

	void requestAndResetInvestments(worker_id_t id, std::vector<InvestmentRequest*> &invs) {
		if (reqsInvest[id].size() > 0) {
			invs.assign(reqsInvest[id].begin(), reqsInvest[id].end());

			reqsInvest[id].clear();
		} else {
			invs.clear();
		}
	}
};

}

}

#endif /* LOADBALANCER_H_ */
