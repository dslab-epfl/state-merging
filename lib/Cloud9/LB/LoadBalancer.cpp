/*
 * LoadBalancer.cpp
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#include "cloud9/lb/LoadBalancer.h"
#include "cloud9/lb/Worker.h"

namespace cloud9 {

namespace lb {

LoadBalancer::LoadBalancer() {
	// TODO Auto-generated constructor stub

}

LoadBalancer::~LoadBalancer() {
	// TODO Auto-generated destructor stub
}

void LoadBalancer::registerWorker(int id) {
	Worker* worker = new Worker();
	worker->id = id;

	workers[id] = worker;
}

void LoadBalancer::deregisterWorker(int id) {
	// TODO
}

void LoadBalancer::updateWorkerStatNodes(std::vector<LBTree::Node*> &newNodes) {

}

void LoadBalancer::updateWorkerStats(std::vector<int> &stats) {

}

}

}
