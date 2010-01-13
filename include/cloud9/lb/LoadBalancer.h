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


	std::map<int, Worker*> workers;
	std::set<int> reqDetails;
	std::map<int, TransferRequest*> reqTransfer;

	TransferRequest *computeTransfer(int fromID, int toID, int count);

public:
	LoadBalancer();
	virtual ~LoadBalancer();

	int registerWorker(const std::string &address, int port);
	void deregisterWorker(int id);

	void analyzeBalance();

	LBTree *getTree() const { return tree; }

	void updateWorkerStatNodes(int id, std::vector<LBTree::Node*> &newNodes);
	void updateWorkerStats(int id, std::vector<int> &stats);

	const std::map<int, Worker*> &getWorkers() { return workers; }
};

}

}

#endif /* LOADBALANCER_H_ */
