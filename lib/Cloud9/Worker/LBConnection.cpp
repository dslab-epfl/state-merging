/*
 * LBConnection.cpp
 *
 *  Created on: Jan 8, 2010
 *      Author: stefan
 */

#include "cloud9/worker/LBConnection.h"
#include "cloud9/worker/WorkerCommon.h"
#include "cloud9/Logger.h"
#include "cloud9/Protocols.h"
#include "cloud9/CommUtils.h"

#include "llvm/Support/CommandLine.h"

#include <string>

using namespace llvm;
using namespace cloud9::data;

namespace cloud9 {

namespace worker {

LBConnection::LBConnection(boost::asio::io_service &service)
	: resolver(service), socket(service), resendStatNodes(false), id(0) {

	tcp::resolver::query query(LBAddress, LBPort);

	tcp::resolver::iterator it = resolver.resolve(query);
	tcp::resolver::iterator end;

	boost::system::error_code error = boost::asio::error::host_not_found;
	while (error && it != end) {
		socket.close();
		socket.connect(*it, error);
		it++;
	}

	if (error) {
		CLOUD9_ERROR("Could not connect to the load balancer");
		return;
	}

	CLOUD9_INFO("Connected to the load balancer");
}

LBConnection::~LBConnection() {
	// TODO Auto-generated destructor stub
}

void LBConnection::registerWorker() {
	// Prepare the registration message
	WorkerReportMessage message;
	message.set_id(0); // Don't have an ID yet

	WorkerReportMessage_Registration *reg = message.mutable_registration();
	reg->set_address(LocalAddress);
	reg->set_port(LocalPort);

	std::string msgString;
	message.SerializeToString(&msgString);
	embedMessageLength(msgString);

}

void LBConnection::sendUpdates() {
	// Prepare the updates message
	WorkerReportMessage message;
	message.set_id(id);

	if (resendStatNodes) {
		WorkerReportMessage_NodeSetUpdate *setUpdate = message.mutable_nodesetupdate();
	}

	WorkerReportMessage_NodeDataUpdate *dataUpdate = message.mutable_nodedataupdate();

	std::string msgString;
	message.SerializeToString(&msgString);
	embedMessageLength(msgString);
}

}

}
