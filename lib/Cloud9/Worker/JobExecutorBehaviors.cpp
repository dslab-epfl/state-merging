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

void RandomExplorationHandler::onNextStateQuery(ExplorationJob *job,
		WorkerTree::Node *&node) {
	// TODO: Find an algorithm in O(1), instead of O(log n)
	if (job->getFrontier().empty()) {
		node = NULL;
		return;
	}

	WorkerTree::Node *crtNode = job->getJobRoot();

	while ((**crtNode).getSymbolicState() == NULL) {
		int index = (int)theRNG.getBool();

		//CLOUD9_DEBUG(index);

		if (crtNode->getChild(index) == NULL)
			index = 1 - index;

		crtNode = crtNode->getChild(index);
		assert(crtNode);
	}

	assert(job->getFrontier().find(crtNode) != job->getFrontier().end());

	node = crtNode;
}

}

}
