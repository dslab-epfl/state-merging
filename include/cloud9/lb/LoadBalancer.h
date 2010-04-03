/*
 * LoadBalancer.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef LOADBALANCER_H_
#define LOADBALANCER_H_

#include "cloud9/lb/TreeNodeInfo.h"

#include <set>
#include <map>

namespace cloud9 {

class ExecutionPath;

namespace lb {

class Worker;

class TransferRequest {
public:
	int fromID;
	int toID;

	ExecutionPathSetPin paths;
	std::vector<int> counts;
public:
	TransferRequest(int from, int to) : fromID(from), toID(to) { }

};

// TODO: Refactor this into a more generic class
class LoadBalancer {
private:
	LBTree *tree;

	std::string programName;
	unsigned statIDCount;

	unsigned nextID;

	unsigned balanceRate;
	unsigned rounds;

	unsigned activeCount;

	std::map<unsigned, Worker*> workers;
	std::set<unsigned> reqDetails;
	std::map<unsigned, TransferRequest*> reqTransfer;

	std::set<unsigned> reports;

	std::vector<uint64_t> coverageData;
	std::vector<char> coverageUpdates;

	TransferRequest *computeTransfer(int fromID, int toID, int count);

public:
	LoadBalancer(int balanceRate);
	virtual ~LoadBalancer();

	unsigned registerWorker(const std::string &address, int port, bool wantsUpdates);
	void deregisterWorker(int id);

	void registerProgramParams(const std::string &programName, unsigned statIDCount);
	void checkProgramParams(const std::string &programName, unsigned statIDCount);

	void analyzeBalance();

	LBTree *getTree() const { return tree; }

	unsigned getActiveCount() const { return activeCount; }

	void updateWorkerStatNodes(unsigned id, std::vector<LBTree::Node*> &newNodes);
	void updateWorkerStats(unsigned id, std::vector<int> &stats);

	void updateCoverageData(unsigned id, const cov_update_t &data);

	Worker* getWorker(unsigned id) {
		std::map<unsigned, Worker*>::iterator it = workers.find(id);
		if (it == workers.end())
			return NULL;
		else
			return (*it).second;
	}

	int getWorkerCount() { return workers.size(); }

	bool requestAndResetDetails(int id) {
		bool result = reqDetails.count(id) > 0;

		if (result) reqDetails.erase(id);

		return result;
	}

	void getAndResetCoverageUpdates(int id, cov_update_t &data);

	TransferRequest *requestAndResetTransfer(int id) {
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
};

}

}

#endif /* LOADBALANCER_H_ */
