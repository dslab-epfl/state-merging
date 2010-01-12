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

LoadBalancer::LoadBalancer() : nextID(1) {
	tree = new LBTree(2);
}

LoadBalancer::~LoadBalancer() {
	// TODO Auto-generated destructor stub
}

int LoadBalancer::registerWorker(const std::string &address, int port) {
	assert(workers[nextID] == NULL);

	Worker *worker = new Worker();
	worker->id = nextID;
	worker->address = address;
	worker->port = port;

	workers[nextID] = worker;

	nextID++;

	return worker->id;
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
		(**node).workerData.erase(id);
	}

	// Add the new stat nodes

	for (std::vector<LBTree::Node*>::iterator it = newNodes.begin();
			it != newNodes.end(); it++) {
		LBTree::Node *node = *it;

		// Update upstream information
		while ((**node).workerData.find(id) == (**node).workerData.end()) {
			(**node).workerData[id] = TreeNodeInfo::WorkerInfo();
			node = node->getParent();
			if (!node)
				break;
		}

		node = *it;
		assert((**node).workerData.size() >= 1);

		if ((**node).workerData.size() > 1) {
			// Request details from all parts
			for (std::map<int, TreeNodeInfo::WorkerInfo>::iterator it =
					(**node).workerData.begin(); it != (**node).workerData.end();
					it++) {

				reqDetails.insert((*it).first);
			}
		}
	}

	// Remove hanging branches
	for (std::vector<LBTree::Node*>::iterator it = worker->nodes.begin();
			it != worker->nodes.end(); it++) {

		LBTree::Node *node = *it;

		// Skip nodes that are part of the new statistics
		if ((**node).workerData.find(id) != (**node).workerData.end())
			continue;

		int crtIndex = node->getIndex();
		node = node->getParent();

		while (node) {
			LBTree::Node *sibling = node->getChild(1 - crtIndex);
			if ((**sibling).workerData.find(id) != (**node).workerData.end())
				break;

			(**node).workerData.erase(id);

			crtIndex = node->getIndex();
			node = node->getParent();
		}
	}

	// Update the list of stat nodes
	worker->nodes = newNodes;
}

void LoadBalancer::updateWorkerStats(int id, std::vector<int> &stats) {
	Worker *worker = workers[id];
	assert(worker);

	assert(stats.size() == worker->nodes.size());

	worker->totalJobs = 0;

	for (int i = 0; i < stats.size(); i++) {
		LBTree::Node *node = worker->nodes[i];

		(**node).workerData[id].jobCount = stats[i];
		worker->totalJobs += stats[i];
	}
}

void LoadBalancer::analyzeBalance() {
	if (workers.size() < 2) {
		return;
	}

	// Compute average and deviation
	int loadAvg = 0;
	int sqDeviation = 0;

	for (std::map<int, Worker*>::iterator it = workers.begin();
			it != workers.end(); it++) {
		loadAvg += (*it).second->totalJobs;
	}

	loadAvg /= workers.size();

	for (std::map<int, Worker*>::iterator it = workers.begin();
			it != workers.end(); it++) {
		sqDeviation += (loadAvg - (*it).second->totalJobs) *
				(loadAvg - (*it).second->totalJobs);
	}

	sqDeviation /= workers.size() - 1;
}

}

}
