/*
 * JobExecutorBehaviors.cpp
 *
 *  Created on: Jan 5, 2010
 *      Author: stefan
 */


#include "cloud9/worker/JobExecutorBehaviors.h"
#include "cloud9/worker/WorkerCommon.h"
#include "cloud9/Logger.h"

#include "klee/Internal/ADT/RNG.h"

using namespace klee;

namespace cloud9 {

namespace worker {

void RandomPathExplorationHandler::onNextStateQuery(ExplorationJob *job,
		WorkerTree::Node *&node) {
	// TODO: Find an algorithm in O(1), instead of O(log n)
	if (job->getFrontier().empty()) {
		node = NULL;
		return;
	}

	WorkerTree::Node *crtNode = job->getJobRoot();

	while ((**crtNode).getSymbolicState() == NULL) {
		int index = (int)theRNG.getBool();

		if (crtNode->getChild(WORKER_LAYER_JOBS, index) == NULL)
			index = 1 - index;

		crtNode = crtNode->getChild(WORKER_LAYER_JOBS, index);

		if (!crtNode) {
			CLOUD9_ERROR("Found empty state inside job; " <<
					job->getFrontier().size() << " " <<
					job->getSize() << " " << job->getDepth());
		}
		assert(crtNode);
	}

	assert(job->getFrontier().find(crtNode) != job->getFrontier().end() && 
	       "crtNode is not on the frontier ... ");

	node = crtNode;
}
}

}
