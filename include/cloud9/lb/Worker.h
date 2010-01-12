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
private:
	int id;
	std::string address;
	int port;

	std::vector<LBTree::Node*> nodes;

	Worker();
public:
	virtual ~Worker();

	int getID() const { return id; }

	const std::string &getAddress() const { return address; }
	int getPort() const { return port; }

	bool operator< (const Worker& w) {
		return id < w.id;
	}
};

}

}

#endif /* WORKER_H_ */
