/*
 * ExplorationJob.cpp
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#include "cloud9/worker/ExplorationJob.h"

namespace cloud9 {

namespace worker {

ExplorationJob::ExplorationJob(WorkerTree::Node *r, bool f)
		: size(1),
		  depth(1),
		  operations(0),
		  started(false),
		  finished(false),
		  foreign(f) {

	jobRoot = r->pin();
	frontier.insert(jobRoot.get());
}

ExplorationJob::~ExplorationJob() {
}

void ExplorationJob::addToFrontier(WorkerTree::Node *node) {
	assert(frontier.find(node) == frontier.end());

	frontier.insert(node);

	// The only way to grow is to add to the frontier
	size++;
	int nodeDepth = node->getLevel() - jobRoot->getLevel() + 1;

	if (nodeDepth > depth)
		depth = nodeDepth;
}

void ExplorationJob::removeFromFrontier(WorkerTree::Node *node) {
	if (frontier.find(node) == frontier.end()) {
		// A state was removed from outside the current job.
		CLOUD9_DEBUG("State removed from outside current job. Broken replays may happen in the future.");
		return;
	}

	frontier.erase(node);
}

}
}
