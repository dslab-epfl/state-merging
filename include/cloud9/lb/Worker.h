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
private:
	int id;
	std::vector<LBTree::Node*> nodes;
public:
	Worker();
	virtual ~Worker();

	int getID() const { return id; }

	bool operator< (const Worker& w) {
		return id < w.id;
	}
};

}

}

#endif /* WORKER_H_ */
