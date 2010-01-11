/*
 * WorkerConnection.cpp
 *
 *  Created on: Jan 7, 2010
 *      Author: stefan
 */

#include "cloud9/lb/WorkerConnection.h"
#include "cloud9/lb/LoadBalancer.h"
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
	// TODO Auto-generated destructor stub
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

		if (!message.ParseFromString(msgString)) {
			CLOUD9_ERROR("Could not parse message contents");
		} else {
			LBResponseMessage response;

			int id = message.id();

			if (id == 0) {
				id = lb->registerWorker();
			}

			if (message.has_nodesetupdate()) {
				const WorkerReportMessage_NodeSetUpdate &nodeSetUpdateMsg =
						message.nodesetupdate();

				processNodeSetUpdate(id, nodeSetUpdateMsg, response);
			}

			if (message.has_nodedataupdate()) {
				const WorkerReportMessage_NodeDataUpdate &nodeDataUpdateMsg =
						message.nodedataupdate();

				processNodeDataUpdate(id, nodeDataUpdateMsg, response);
			}

			std::string respString;
			response.SerializeToString(&respString);

			msgWriter.sendMessage(respString);
		}

	} else {
		CLOUD9_ERROR("Could not fully read message contents");
	}
}

void WorkerConnection::handleMessageSent(const boost::system::error_code &error) {
	if (!error) {

	} else {
		CLOUD9_ERROR("Could not send reply");
	}

	// Wait for another message, again
	msgReader.recvMessage();
}


void WorkerConnection::processNodeSetUpdate(int id,
				const WorkerReportMessage_NodeSetUpdate &message,
				LBResponseMessage &response) {

	std::vector<LBTree::Node*> nodes;
	std::vector<ExecutionPath*> paths;

	parseExecutionPathSet(message.pathset(), paths);

	lb->getTree()->getNodes(paths, nodes);

	lb->updateWorkerStatNodes(id, nodes);

}

void WorkerConnection::processNodeDataUpdate(int id,
		const WorkerReportMessage_NodeDataUpdate &message,
		LBResponseMessage &response) {
	std::vector<int> data;

	data.insert(data.begin(), message.data().begin(), message.data().end());

	lb->updateWorkerStats(id, data);
}

}

}
