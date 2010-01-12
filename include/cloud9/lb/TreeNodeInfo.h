/*
 * TreeNodeInfo.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef TREENODEINFO_H_
#define TREENODEINFO_H_

#include "cloud9/ExecutionTree.h"

#include <set>
#include <map>

namespace cloud9 {

namespace lb {

class Worker;

class TreeNodeInfo {
	friend class LoadBalancer;
public:
	struct WorkerInfo {
		int jobCount;
	};

private:
	std::map<int, WorkerInfo> workerData;
public:
	TreeNodeInfo() {};
	virtual ~TreeNodeInfo() {};
};


typedef ExecutionTree<TreeNodeInfo> LBTree;

}

}


#endif /* TREENODEINFO_H_ */
