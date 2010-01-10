/*
 * LBConnection.cpp
 *
 *  Created on: Jan 8, 2010
 *      Author: stefan
 */

#include "cloud9/worker/LBConnection.h"
#include "cloud9/worker/WorkerCommon.h"
#include "cloud9/worker/JobManager.h"
#include "cloud9/Logger.h"
#include "cloud9/Protocols.h"

#include "llvm/Support/CommandLine.h"

#include <string>

using namespace llvm;
using namespace cloud9::data;

namespace cloud9 {

namespace worker {

LBConnection::LBConnection(boost::asio::io_service &s,
		JobManager *jm)
	: service(s), socket(s), jobManager(jm), id(0) {

	boost::system::error_code error;
	connectSocket(service, socket, LBAddress, LBPort, error);

	if (error) {
		CLOUD9_ERROR("Could not connect to the load balancer");
		return;
	}

	CLOUD9_INFO("Connected to the load balancer");
}

LBConnection::~LBConnection() {
	socket.close();
}

void LBConnection::registerWorker() {
	// Prepare the registration message
	WorkerReportMessage message;
	message.set_id(0); // Don't have an ID yet

	WorkerReportMessage_Registration *reg = message.mutable_registration();
	reg->set_address(LocalAddress);
	reg->set_port(LocalPort);

	// Send the message
	std::string msgString;
	message.SerializeToString(&msgString);
	sendMessage(socket, msgString);

	std::string respString;
	// Wait for the response
	recvMessage(socket, respString);

	LBResponseMessage response;
	response.ParseFromString(respString);

	id = response.id();

	CLOUD9_INFO("Worker registered with the load balancer - ID: " << id);
}

void LBConnection::sendUpdates() {
	// Prepare the updates message
	WorkerReportMessage message;
	message.set_id(id);

	WorkerReportMessage_NodeDataUpdate *dataUpdate = message.mutable_nodedataupdate();
	std::vector<int> data;
	jobManager->getStatisticsData(data);

	for (std::vector<int>::iterator it = data.begin(); it != data.end(); it++) {
		dataUpdate->add_data(*it);
	}

	if (jobManager->isStatStructureChanged()) {
			WorkerReportMessage_NodeSetUpdate *setUpdate = message.mutable_nodesetupdate();
			ExecutionPathSet *pathSet = setUpdate->mutable_pathset();

			std::vector<ExecutionPath*> paths;
			jobManager->getStatisticsNodes(paths);

			assert(paths.size() == data.size());

			serializeExecutionPathSet(paths, *pathSet);

			jobManager->resetStatStructureChanged();
		}

	std::string msgString;
	message.SerializeToString(&msgString);
	sendMessage(socket, msgString);

	std::string respString;
	recvMessage(socket, respString);

	LBResponseMessage response;
	response.ParseFromString(respString);

	assert(id == response.id());

	if (response.more_details()) {
		jobManager->refineStatistics();
	}

	if (response.has_statetransfer()) {
		const LBResponseMessage_StateTransfer &transDetails =
				response.statetransfer();

		int jobCount = transDetails.count();
		std::string destAddress = transDetails.dest_address();
		int destPort = transDetails.dest_port();

		transferJobs(jobCount, destAddress, destPort);
	}
}

void LBConnection::transferJobs(int jobCount, std::string &address,
		int port) {
	std::vector<ExecutionPath*> paths;

	jobManager->exportJobs(jobCount, paths);

	tcp::socket peerSocket(service);
	boost::system::error_code error;

	connectSocket(service, peerSocket, address, port, error);

	if (error) {
		CLOUD9_ERROR("Could not connect to the peer worker");
		return;
	}

	PeerTransferMessage message;
	ExecutionPathSet *pSet = message.mutable_path_set();

	serializeExecutionPathSet(paths, *pSet);

	std::string msgString;
	message.SerializeToString(&msgString);

	sendMessage(peerSocket, msgString);

	peerSocket.close();
}

}

}
