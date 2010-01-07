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


	std::map<int, Worker*> workers;

	void registerWorker(int id);
	void deregisterWorker(int id);

	void updateWorkerStatNodes(std::vector<LBTree::Node*> &newNodes);
	void updateWorkerStats(std::vector<int> &stats);
public:
	LoadBalancer();
	virtual ~LoadBalancer();
};

}

}

#endif /* LOADBALANCER_H_ */
