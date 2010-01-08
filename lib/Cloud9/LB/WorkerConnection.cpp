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
		: socket(service), lb(_lb) {

	// TODO Auto-generated constructor stub

}

WorkerConnection::~WorkerConnection() {
	// TODO Auto-generated destructor stub
}

void WorkerConnection::readMessageHeader() {
	boost::asio::async_read(socket,
			boost::asio::buffer(&msgSize, sizeof(msgSize)),
			boost::bind(&WorkerConnection::readMessageContents, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
}

void WorkerConnection::readMessageContents(const boost::system::error_code &error, size_t size) {
	if (!error) {
		assert(size == sizeof(msgSize));

		msgData = new char[msgSize];

		boost::asio::async_read(socket,
				boost::asio::buffer(msgData, msgSize),
				boost::bind(&WorkerConnection::processMessage, this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
	} else {
		CLOUD9_ERROR("Could not read message header");
	}
}

void WorkerConnection::processMessage(const boost::system::error_code &error, size_t size) {
	if (!error) {
		assert(size == msgSize);

		// Construct the protocol buffer message
		WorkerReportMessage message;

		if (!message.ParseFromArray(msgData, msgSize)) {
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
			size_t respSize = respString.size();
			respString.insert(0, (char*)&respSize, sizeof(respSize));

			boost::asio::async_write(socket,
					boost::asio::buffer(respString),
					boost::bind(&WorkerConnection::finishMessageHandling,
							this, boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred));
		}

	} else {
		CLOUD9_ERROR("Could not fully read message contents");
	}
}

void WorkerConnection::finishMessageHandling(const boost::system::error_code &error, size_t) {
	if (error) {
		CLOUD9_ERROR("Could not set worker reply");
	}

	if (msgData) {
		delete[] msgData;
		msgData = NULL;
		msgSize = 0;
	}

	// Start over
	readMessageHeader();
}


void WorkerConnection::processNodeSetUpdate(int id,
				const WorkerReportMessage_NodeSetUpdate &message,
				LBResponseMessage &response) {

	std::vector<LBTree::Node*> nodes;
	std::vector<ExecutionPath*> paths;

	ExecutionPath::parseExecutionPathSet(message.pathset(), paths);

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
