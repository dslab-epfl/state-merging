/*
 * JobExecutor.cpp
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#include "JobExecutor.h"

namespace cloud9 {

namespace worker {


JobExecutor::JobExecutor() {
	// TODO Auto-generated constructor stub

}

JobExecutor::~JobExecutor() {
	// TODO Auto-generated destructor stub
}

WorkerTree::Node *JobExecutor::getNextNode() {
	return NULL;
}

void JobExecutor::exploreNode(WorkerTree::Node *node) {

}

void JobExecutor::executeJob(ExplorationJob *job) {

}

}
}
