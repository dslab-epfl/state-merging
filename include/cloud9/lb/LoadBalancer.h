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

namespace lb {

class Worker;

class LoadBalancer {
private:
	LBTree *tree;

	int nextID;


	std::map<int, Worker*> workers;
	std::set<int> reqDetails;
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
