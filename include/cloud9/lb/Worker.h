/*
 * Worker.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef WORKER_H_
#define WORKER_H_

#include "cloud9/lb/TreeNodeInfo.h"
#include "cloud9/lb/StrategyStatistic.h"

#include <vector>
#include <map>

namespace cloud9 {

namespace lb {

typedef unsigned int worker_id_t;

class Worker {
	friend class LoadBalancer;
public:
	struct IDCompare {
		bool operator() (const Worker *a, const Worker *b) {
			return a->getID() < b->getID();
		}
	};

	struct LoadCompare {
		bool operator() (const Worker *a, const Worker *b) {
			return a->getTotalJobs() < b->getTotalJobs();
		}
	};
private:
	worker_id_t id;
	std::string address;
	int port;

	bool _wantsUpdates;

	std::vector<LBTree::Node*> nodes;
	unsigned nodesRevision;

	std::vector<char> coverageUpdates;

	std::vector<StrategyStatistic> strategyStatistics;
	
	unsigned totalJobs;

	Worker() : nodesRevision(1), totalJobs(0) { };
public:
	virtual ~Worker() { };

	worker_id_t getID() const { return id; }

	unsigned getTotalJobs() const { return totalJobs; }

	const std::string &getAddress() const { return address; }
	int getPort() const { return port; }

	bool operator< (const Worker& w) {
		return id < w.id;
	}

	bool wantsUpdates() const {
		return _wantsUpdates;
	}
};

}

}

#endif /* WORKER_H_ */
