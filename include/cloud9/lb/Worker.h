/*
 * Worker.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef WORKER_H_
#define WORKER_H_

#include "cloud9/lb/TreeNodeInfo.h"

#include <vector>

namespace cloud9 {

namespace lb {


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
	int id;
	std::string address;
	int port;

	std::vector<LBTree::Node*> nodes;
	int nodesRevision;

	int totalJobs;



	Worker() : nodesRevision(1), totalJobs(0) { };
public:
	virtual ~Worker() { };

	int getID() const { return id; }

	int getTotalJobs() const { return totalJobs; }

	const std::string &getAddress() const { return address; }
	int getPort() const { return port; }

	bool operator< (const Worker& w) {
		return id < w.id;
	}
};

}

}

#endif /* WORKER_H_ */
