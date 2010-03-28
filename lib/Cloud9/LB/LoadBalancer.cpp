/*
 * LoadBalancer.cpp
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#include "cloud9/lb/LoadBalancer.h"
#include "cloud9/lb/Worker.h"

#include <cassert>
#include <algorithm>

namespace cloud9 {

namespace lb {

LoadBalancer::LoadBalancer(int br) : nextID(1), balanceRate(br), rounds(0) {
	tree = new LBTree(2);
}

LoadBalancer::~LoadBalancer() {
	// TODO Auto-generated destructor stub
}

void LoadBalancer::registerProgramParams(const std::string &programName,
		unsigned statIDCount) {
	this->programName = programName;
	this->statIDCount = statIDCount;

	coverageData.resize(statIDCount);
	coverageUpdates.resize(statIDCount, false);
}

void LoadBalancer::checkProgramParams(const std::string &programName,
		unsigned statIDCount) {
	assert(this->programName == programName);
	assert(this->statIDCount == statIDCount);
}

unsigned LoadBalancer::registerWorker(const std::string &address, int port) {
	assert(workers[nextID] == NULL);

	Worker *worker = new Worker();
	worker->id = nextID;
	worker->address = address;
	worker->port = port;
	worker->coverageUpdates = coverageUpdates;

	workers[nextID] = worker;

	nextID++;

	return worker->id;
}

void LoadBalancer::deregisterWorker(int id) {
	Worker *worker = workers[id];
	assert(worker);

	// TODO
}

void LoadBalancer::updateWorkerStatNodes(unsigned id, std::vector<LBTree::Node*> &newNodes) {
	Worker *worker = workers[id];
	assert(worker);

	int revision = worker->nodesRevision++;

	// Add the new stat nodes
	for (std::vector<LBTree::Node*>::iterator it = newNodes.begin();
			it != newNodes.end(); it++) {
		LBTree::Node *node = *it;

		// Update upstream information
		while (node) {
			TreeNodeInfo::WorkerInfo &info = (**node).workerData[id];
			if (info.revision > 0 && info.revision == revision)
				break;

			info.revision = revision;

			node = node->getParent();
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

	// Remove old branches
	for (std::vector<LBTree::Node*>::iterator it = worker->nodes.begin();
			it != worker->nodes.end(); it++) {

		LBTree::Node *node = *it;

		while (node) {
			if ((**node).workerData.find(id) == (**node).workerData.end())
				break;

			TreeNodeInfo::WorkerInfo &info = (**node).workerData[id];

			assert(info.revision > 0);

			if (info.revision == revision)
				break;

			(**node).workerData.erase(id);

			node = node->getParent();
		}
	}

	// Update the list of stat nodes
	worker->nodes = newNodes;
}

void LoadBalancer::updateWorkerStats(unsigned id, std::vector<int> &stats) {
	Worker *worker = workers[id];
	assert(worker);

	assert(stats.size() == worker->nodes.size());

	worker->totalJobs = 0;

	for (unsigned i = 0; i < stats.size(); i++) {
		LBTree::Node *node = worker->nodes[i];

		(**node).workerData[id].jobCount = stats[i];
		worker->totalJobs += stats[i];
	}

	reports.insert(id);

	if (reports.size() == workers.size()) {
		// A full round finished
		reports.clear();
		rounds++;
	}
}

void LoadBalancer::updateCoverageData(unsigned id, const cov_update_t &data) {
	for (cov_update_t::const_iterator it = data.begin(); it != data.end(); it++) {
		coverageUpdates[it->first] = true;
		coverageData[it->first] = it->second;
	}

	for (std::map<unsigned, Worker*>::iterator wIt = workers.begin();
				wIt != workers.end(); wIt++) {
		if (wIt->first == id)
			continue;

		for (cov_update_t::const_iterator it = data.begin(); it != data.end(); it++) {
			wIt->second->coverageUpdates[it->first] = true;
		}
	}
}

void LoadBalancer::getAndResetCoverageUpdates(int id, cov_update_t &data) {
	Worker *w = workers[id];

	for (unsigned i = 0; i < w->coverageUpdates.size(); i++) {
		if (w->coverageUpdates[i]) {
			data.push_back(std::make_pair(i, coverageData[i]));
			w->coverageUpdates[i] = false;
		}
	}
}

void LoadBalancer::analyzeBalance() {
	if (workers.size() < 2) {
		return;
	}

	if (rounds < balanceRate)
		return;

	rounds = 0;

	CLOUD9_INFO("Performing load balancing");

	std::vector<Worker*> wList;
	Worker::LoadCompare comp;

	// TODO: optimize this further
	for (std::map<unsigned, Worker*>::iterator it = workers.begin();
			it != workers.end(); it++) {
		wList.push_back((*it).second);
	}

	std::sort(wList.begin(), wList.end(), comp);

	// Compute average and deviation
	int loadAvg = 0;
	int sqDeviation = 0;

	for (std::vector<Worker*>::iterator it = wList.begin();
			it != wList.end(); it++) {
		loadAvg += (*it)->totalJobs;
	}

	loadAvg /= wList.size();

	for (std::vector<Worker*>::iterator it = wList.begin();
			it != wList.end(); it++) {
		sqDeviation += (loadAvg - (*it)->totalJobs) *
				(loadAvg - (*it)->totalJobs);
	}

	sqDeviation /= workers.size() - 1;

	// XXX Uuuugly


	std::vector<Worker*>::iterator lowLoadIt = wList.begin();
	std::vector<Worker*>::iterator highLoadIt = wList.end() - 1;

	while (lowLoadIt < highLoadIt) {
		if (reqTransfer.count((*lowLoadIt)->id) > 0) {
			lowLoadIt++;
			continue;
		}
		if (reqTransfer.count((*highLoadIt)->id) > 0) {
			highLoadIt--;
			continue;
		}

		if ((*lowLoadIt)->totalJobs * 10 < loadAvg) {
			TransferRequest *req = computeTransfer((*highLoadIt)->id,
					(*lowLoadIt)->id,
					((*highLoadIt)->totalJobs - (*lowLoadIt)->totalJobs)/2);

			reqTransfer[(*lowLoadIt)->id] = req;
			reqTransfer[(*highLoadIt)->id] = req;

			highLoadIt--;
			lowLoadIt++;
			continue;
		} else
			break; // The next ones will have a larger load anyway
	}


}

TransferRequest *LoadBalancer::computeTransfer(int fromID, int toID, int count) {
	// XXX Be more intelligent
	TransferRequest *req = new TransferRequest(fromID, toID);

	req->counts.push_back(count);

	std::vector<LBTree::Node*> nodes;
	nodes.push_back(tree->getRoot());

	req->paths = tree->buildPathSet(nodes.begin(), nodes.end());

	CLOUD9_DEBUG("Created transfer request from " << fromID << " to " <<
				toID << " for " << count << " states");

	return req;
}

}

}
