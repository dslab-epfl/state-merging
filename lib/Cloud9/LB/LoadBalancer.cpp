/*
 * LoadBalancer.cpp
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#include "cloud9/lb/LoadBalancer.h"
#include "cloud9/lb/Worker.h"

#include <cassert>

namespace cloud9 {

namespace lb {

LoadBalancer::LoadBalancer() {
	// TODO Auto-generated constructor stub

}

LoadBalancer::~LoadBalancer() {
	// TODO Auto-generated destructor stub
}

void LoadBalancer::registerWorker(int id) {
	assert(workers[id] == NULL);

	Worker *worker = new Worker();
	worker->id = id;

	workers[id] = worker;
}

void LoadBalancer::deregisterWorker(int id) {
	Worker *worker = workers[id];
	assert(worker);

	// TODO
}

void LoadBalancer::updateWorkerStatNodes(int id, std::vector<LBTree::Node*> &newNodes) {
	Worker *worker = workers[id];
	assert(worker);

	// Remove the old stat nodes
	for (std::vector<LBTree::Node*>::iterator it = worker->nodes.begin();
			it != worker->nodes.end(); it++) {
		LBTree::Node *node = *it;

		// Remove the worker from the node stats
		(**node).workerData.erase(worker);
	}

	// Add the new stat nodes
	worker->nodes.clear();
	worker->nodes.insert(worker->nodes.begin(), newNodes.begin(), newNodes.end());

	for (std::vector<LBTree::Node*>::iterator it = worker->nodes.begin();
			it != worker->nodes.end(); it++) {
		LBTree::Node *node = *it;

		(**node).workerData[worker] = TreeNodeInfo::WorkerInfo();
	}
}

void LoadBalancer::updateWorkerStats(int id, std::vector<int> &stats) {
	Worker *worker = workers[id];
	assert(worker);

	assert(stats.size() == worker->nodes.size());

	for (int i = 0; i < stats.size(); i++) {
		LBTree::Node *node = worker->nodes[i];

		(**node).workerData[worker].jobCount = stats[i];
	}
}

}

}
