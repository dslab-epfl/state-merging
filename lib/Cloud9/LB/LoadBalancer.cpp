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
	// Remove the old stats
	Worker *worker = workers[id];
	assert(worker);

	for (std::vector<LBTree::Node*>::iterator it = worker->nodes.begin();
			it != worker->nodes.end(); it++) {
		LBTree::Node *node = *it;

		(**node).workerData.erase(worker);
	}
}

void LoadBalancer::updateWorkerStats(int id, std::vector<int> &stats) {

}

}

}
