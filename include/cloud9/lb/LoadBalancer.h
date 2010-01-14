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

	std::vector<ExecutionPath*> paths;
	std::vector<int> counts;
public:
	TransferRequest(int from, int to) : fromID(from), toID(to) { }

};

// TODO: Refactor this into a more generic class
class LoadBalancer {
private:
	LBTree *tree;

	int nextID;

	int balanceRate;
	int rounds;


	std::map<int, Worker*> workers;
	std::set<int> reqDetails;
	std::map<int, TransferRequest*> reqTransfer;

	std::set<int> reports;

	TransferRequest *computeTransfer(int fromID, int toID, int count);

public:
	LoadBalancer(int balanceRate);
	virtual ~LoadBalancer();

	int registerWorker(const std::string &address, int port);
	void deregisterWorker(int id);

	void analyzeBalance();

	LBTree *getTree() const { return tree; }

	void updateWorkerStatNodes(int id, std::vector<LBTree::Node*> &newNodes);
	void updateWorkerStats(int id, std::vector<int> &stats);

	Worker* getWorker(int id) {
		std::map<int, Worker*>::iterator it = workers.find(id);
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
