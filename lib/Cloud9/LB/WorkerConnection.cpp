/*
 * WorkerConnection.cpp
 *
 *  Created on: Jan 7, 2010
 *      Author: stefan
 */

#include "cloud9/lb/WorkerConnection.h"
#include "cloud9/lb/LoadBalancer.h"
#include "cloud9/lb/Worker.h"
#include "cloud9/Logger.h"

#include <boost/bind.hpp>
#include <string>
#include <vector>

using namespace cloud9::data;

namespace cloud9 {

namespace lb {

WorkerConnection::WorkerConnection(boost::asio::io_service &service, LoadBalancer *_lb)
		: socket(service), lb(_lb),
		  msgReader(socket), msgWriter(socket) {

}

WorkerConnection::~WorkerConnection() {
	CLOUD9_INFO("Connection interrupted");
}

void WorkerConnection::start() {
	msgReader.setHandler(boost::bind(&WorkerConnection::handleMessageReceived,
			shared_from_this(), _1, _2));
	msgWriter.setHandler( boost::bind(&WorkerConnection::handleMessageSent,
			shared_from_this(), _1));

	msgReader.recvMessage();
}

void WorkerConnection::handleMessageReceived(std::string &msgString,
			const boost::system::error_code &error) {
	if (!error) {
		// Construct the protocol buffer message
		WorkerReportMessage message;

		bool result = message.ParseFromString(msgString);
		assert(result);

		LBResponseMessage response;

		int id = message.id();

		if (id == 0) {
			const WorkerReportMessage_Registration &regInfo =
					message.registration();

			if (lb->getWorkerCount() == 0) {
				// The first worker sets the program parameters
				lb->registerProgramParams(regInfo.prog_name(), regInfo.stat_id_count());
			} else {
				// Validate that the worker executes the same thing as the others
				lb->checkProgramParams(regInfo.prog_name(), regInfo.stat_id_count());
			}

			id = lb->registerWorker(regInfo.address(), regInfo.port());

			response.set_id(id);
			response.set_more_details(false);

			if (lb->getWorkerCount() == 1) {
				// Send the seed information
				LBResponseMessage_JobSeed *jobSeed = response.mutable_jobseed();
				cloud9::data::ExecutionPathSet *pathSet = jobSeed->mutable_path_set();

				std::vector<LBTree::Node*> nodes;
				nodes.push_back(lb->getTree()->getRoot());

				ExecutionPathSetPin paths =
						lb->getTree()->buildPathSet(nodes.begin(), nodes.end());

				serializeExecutionPathSet(paths, *pathSet);
			}
		} else {
			processNodeSetUpdate(message);
			processNodeDataUpdate(message);
			processStatisticsUpdates(message);

			lb->analyzeBalance();

			response.set_id(id);
			response.set_more_details(lb->requestAndResetDetails(id));

			sendJobTransfers(response);
			sendStatisticsUpdates(response);
		}

		std::string respString;
		result = response.SerializeToString(&respString);
		assert(result);

		msgWriter.sendMessage(respString);


	} else {
		CLOUD9_ERROR("Error receiving message from worker");
	}
}

void WorkerConnection::sendJobTransfers(LBResponseMessage &response) {
	unsigned id = response.id();
	TransferRequest *transfer = lb->requestAndResetTransfer(id);

	if (transfer) {
		const Worker *destination = lb->getWorker(transfer->toID);

		LBResponseMessage_JobTransfer *transMsg =
				response.mutable_jobtransfer();

		transMsg->set_dest_address(destination->getAddress());
		transMsg->set_dest_port(destination->getPort());

		cloud9::data::ExecutionPathSet *pathSet = transMsg->mutable_path_set();
		serializeExecutionPathSet(transfer->paths, *pathSet);

		for (std::vector<int>::iterator it = transfer->counts.begin();
				it != transfer->counts.end(); it++) {
			transMsg->add_count(*it);
		}

		delete transfer;
	}
}

void WorkerConnection::sendStatisticsUpdates(LBResponseMessage &response) {
	unsigned id = response.id();
	cov_update_t data;

	lb->getAndResetCoverageUpdates(id, data);

	if (data.size() > 0) {
		StatisticUpdate *update = response.add_globalupdates();
		serializeStatisticUpdate(CLOUD9_STAT_NAME_GLOBAL_COVERAGE, data, *update);
	}

}

void WorkerConnection::handleMessageSent(const boost::system::error_code &error) {
	if (!error) {
		//CLOUD9_DEBUG("Sent reply to worker");
	} else {
		CLOUD9_ERROR("Could not send reply");
	}

	// Wait for another message, again
	msgReader.recvMessage();
}

void WorkerConnection::processStatisticsUpdates(const WorkerReportMessage &message) {
	unsigned id = message.id();

	for (int i = 0; i < message.localupdates_size(); i++) {
		const StatisticUpdate &update = message.localupdates(i);

		if (update.name() == CLOUD9_STAT_NAME_LOCAL_COVERAGE) {
			cov_update_t data;
			parseStatisticUpdate(update, data);

			if (data.size() > 0)
				lb->updateCoverageData(id, data);
		}
	}
}


void WorkerConnection::processNodeSetUpdate(const WorkerReportMessage &message) {
	if (!message.has_nodesetupdate())
		return;

	unsigned id = message.id();
	const WorkerReportMessage_NodeSetUpdate &nodeSetUpdateMsg =
			message.nodesetupdate();

	std::vector<LBTree::Node*> nodes;
	ExecutionPathSetPin paths = parseExecutionPathSet(nodeSetUpdateMsg.pathset());

	lb->getTree()->getNodes(paths, nodes);

	CLOUD9_DEBUG("Received node set: " << getASCIINodeSet(nodes.begin(),
			nodes.end()));

	lb->updateWorkerStatNodes(id, nodes);

}

void WorkerConnection::processNodeDataUpdate(const WorkerReportMessage &message) {
	if (!message.has_nodedataupdate())
		return;

	unsigned id = message.id();
	const WorkerReportMessage_NodeDataUpdate &nodeDataUpdateMsg =
			message.nodedataupdate();

	std::vector<int> data;

	data.insert(data.begin(), nodeDataUpdateMsg.data().begin(),
			nodeDataUpdateMsg.data().end());

	CLOUD9_DEBUG("Received data set: " << getASCIIDataSet(data.begin(), data.end()));

	lb->updateWorkerStats(id, data);
}

}

}
