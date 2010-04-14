/*
 * JobExecutor.cpp
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#include "cloud9/worker/JobExecutor.h"
#include "cloud9/worker/SymbolicEngine.h"
#include "cloud9/worker/WorkerCommon.h"
#include "cloud9/worker/KleeCommon.h"
#include "cloud9/Logger.h"
#include "cloud9/Utils.h"
#include "cloud9/instrum/InstrumentationManager.h"
#include "cloud9/instrum/InstrumentationWriter.h"
#include "cloud9/instrum/LocalFileWriter.h"

#include "llvm/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Instructions.h"

using namespace llvm;
using namespace klee;
using namespace klee::c9;

namespace cloud9 {

namespace worker {


unsigned JobExecutor::getModuleCRC() const {
	std::string moduleContents;
	llvm::raw_string_ostream os(moduleContents);

	finalModule->module->print(os, NULL);
	os.flush();

	boost::crc_ccitt_type crc;
	crc.process_bytes(moduleContents.c_str(), moduleContents.size());

	return crc.checksum();
}

void JobExecutor::initRootState(llvm::Function *f, int argc,
			char **argv, char **envp) {
	klee::ExecutionState *state = symbEngine->createRootState(f);
	WorkerTree::NodePin node = tree->getRoot()->pin(WORKER_LAYER_STATES);

	(**node).symState = state;
	state->setWorkerNode(node);

	symbEngine->initRootState(state, argc, argv, envp);
}

void JobExecutor::finalizeExecution() {
	symbEngine->deregisterStateEventHandler(this);
	symbEngine->destroyStates();
}

WorkerTree::Node *JobExecutor::getNextNode() {
	WorkerTree::Node *nextNode = NULL;

	expHandler->onNextStateQuery(currentJob, nextNode);

	return nextNode;
}

void JobExecutor::exploreNode(WorkerTree::Node *node) {
	boost::unique_lock<boost::mutex> lock(executorMutex);
	// Execute instructions until the state is destroyed or branching occurs.
	// When a branching occurs, the node will become empty (as the state and its
	// fork move in its children)
	if ((**node).symState == NULL) {
		CLOUD9_INFO("Exploring empty state!");
	}

	//CLOUD9_DEBUG("Exploring new node!");

	// Keep the node alive until we finish with it
	WorkerTree::NodePin nodePin = node->pin(WORKER_LAYER_STATES);

	if (!currentJob) {
		//CLOUD9_DEBUG("Starting to replay node at position " << *((**node).symState));
	}

	while ((**node).symState != NULL) {

		ExecutionState *state = (**node).symState;

		//CLOUD9_DEBUG("Stepping in instruction " << state->pc->info->assemblyLine);
		if (!codeBreaks.empty()) {
			if (codeBreaks.find(state->pc->info->assemblyLine) !=
					codeBreaks.end()) {
				// We hit a breakpoint
				fireBreakpointHit(node);
			}
		}

		// Execute the instruction
		symbEngine->stepInState(state);
		cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalProcInstructions);

		if (currentJob) {
			currentJob->operations++;
			cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalNewInstructions);
		}
	}

	cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalStatesExplored);


	if (currentJob) {
		cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalNewStates);
	}

}

void JobExecutor::fireBreakpointHit(WorkerTree::Node *node) {
	klee::ExecutionState *state = (**node).symState;

	CLOUD9_INFO("Breakpoint hit!");
	CLOUD9_DEBUG("State at position: " << *node);

	if (state) {
		CLOUD9_DEBUG("State stack trace: " << *state);
		klee::ExprPPrinter::printConstraints(std::cerr, state->constraints);
		dumpStateTrace(node);
	}

	// Also signal a breakpoint, for stopping GDB
	cloud9::breakSignal();
}

void JobExecutor::updateTreeOnBranch(klee::ExecutionState *state,
		klee::ExecutionState *parent, int index) {

	WorkerTree::Node *pNode = parent->getWorkerNode().get();

	WorkerTree::NodePin newNodePin(WORKER_LAYER_STATES), oldNodePin(WORKER_LAYER_STATES);

	// Obtain the new node pointers
	oldNodePin = tree->getNode(WORKER_LAYER_STATES, pNode, 1 - index)->pin(WORKER_LAYER_STATES);

	// Update state -> node references
	parent->setWorkerNode(oldNodePin);

	// Update node -> state references
	(**pNode).symState = NULL;
	(**oldNodePin).symState = parent;

	// Update frontier
	if (currentJob) {
		currentJob->removeFromFrontier(pNode);
		currentJob->addToFrontier(oldNodePin.get());
	}

	if (state) {
		newNodePin = tree->getNode(WORKER_LAYER_STATES, pNode, index)->pin(WORKER_LAYER_STATES);

		state->setWorkerNode(newNodePin);
		(**newNodePin).symState = state;

		if (currentJob) {
			currentJob->addToFrontier(newNodePin.get());
			cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalNewPaths);
		}

		cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalPathsStarted);
		cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::CurrentPathCount);
	}
}

void JobExecutor::updateTreeOnDestroy(klee::ExecutionState *state) {
	WorkerTree::Node *pNode = state->getWorkerNode().get();

	(**pNode).symState = NULL;



	if (currentJob) {
		currentJob->removeFromFrontier(pNode);
	}

	// Unpin the node
	state->getWorkerNode().reset();

	cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalPathsFinished);
	cloud9::instrum::theInstrManager.decStatistic(cloud9::instrum::CurrentPathCount);
}



void JobExecutor::executeJob(ExplorationJob *job) {

	if ((**(job->jobRoot)).symState == NULL) {
		if (!job->foreign) {
			CLOUD9_INFO("Replaying path for non-foreign job. Most probably this job will be lost.");
		}

		cloud9::instrum::theInstrManager.recordEvent(cloud9::instrum::JobExecutionState, "startReplay");

		replayPath(job->jobRoot.get());

		cloud9::instrum::theInstrManager.recordEvent(cloud9::instrum::JobExecutionState, "endReplay");
	} else {
		if (job->foreign) {
			CLOUD9_INFO("Foreign job with no replay needed. Probably state was obtained through other neighbor replays.");
		}
	}

	currentJob = job;
	fireJobStarted(job);

	if ((**(job->jobRoot)).symState == NULL) {
		CLOUD9_INFO("Job canceled before start");
		job->frontier.clear();
		cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalDroppedJobs);
	} else {
		while (!job->frontier.empty()) {
			// Select a new state to explore next
			WorkerTree::Node *node = getNextNode();

			assert(node);

			exploreNode(node);

			bool finish = false;
			sizingHandler->onTerminationQuery(job, finish);

			if (finish)
				break;
		}
	}

	fireJobTerminated(job);
	currentJob = NULL;
}

void JobExecutor::replayPath(WorkerTree::Node *pathEnd) {
	std::vector<int> path;

	WorkerTree::Node *crtNode = pathEnd;

	CLOUD9_DEBUG("Replaying path: " << *crtNode);

	while (crtNode != NULL && (**crtNode).symState == NULL) {
		path.push_back(crtNode->getIndex());

		crtNode = crtNode->getParent();
	}

	if (crtNode == NULL) {
		CLOUD9_ERROR("Cannot find the seed execution state, abandoning the job");
		return;
	}

	/*if (!crtNode) {
		CLOUD9_WARNING("Cloud not replay path: " << *pathEnd);
		return;
	}*/

	std::reverse(path.begin(), path.end());

	// Don't run this as a job - the generated states must
	// not be explored
	currentJob = NULL;

	CLOUD9_DEBUG("Started path replay at position: " << *crtNode);
	WorkerTree::Node *lastValidState = NULL;
	
	// Perform the replay work
	for (unsigned int i = 0; i < path.size(); i++) {
		if ((**crtNode).symState != NULL) {
			lastValidState = crtNode;
			exploreNode(crtNode);
		} else {
			CLOUD9_DEBUG("Potential fast-forward at position " << i <<
					" out of " << path.size() << " in the path.");
		}

		crtNode = crtNode->getChild(WORKER_LAYER_JOBS, path[i]);
		assert(crtNode != NULL);
	}

	if ((**crtNode).symState == NULL) {
		CLOUD9_ERROR("Replay broken, NULL state at the end of the path. Maybe the state went past the job root?");
		if (BreakOnReplayBroken) {
			assert(lastValidState != NULL);
			fireBreakpointHit(lastValidState);
		}
	}
}

}
}
