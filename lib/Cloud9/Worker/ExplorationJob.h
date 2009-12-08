/*
 * ExplorationJob.h
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#ifndef EXPLORATIONJOB_H_
#define EXPLORATIONJOB_H_

#include "cloud9/ExecutionTree.h"
#include "TreeNodeInfo.h"

#include <list>

namespace cloud9 {

namespace worker {

class ExplorationJob {
public:
	typedef ExecutionTree<TreeNodeInfo,TreeArcInfo> WorkerTree;
private:
	WorkerTree *tree;

	int size;
	int depth;

	std::list<WorkerTree::Node> frontier;
public:
	ExplorationJob();
	virtual ~ExplorationJob();
};

}
}

#endif /* EXPLORATIONJOB_H_ */
