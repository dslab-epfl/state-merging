/*
 * PeerServer.cpp
 *
 *  Created on: Jan 9, 2010
 *      Author: stefan
 */

#include "cloud9/worker/PeerServer.h"
#include "cloud9/worker/WorkerCommon.h"
#include "cloud9/worker/JobManager.h"
#include "cloud9/Protocols.h"
#include "cloud9/Logger.h"

#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <vector>

using namespace cloud9::data;

namespace cloud9 {

namespace worker {

PeerConnection::PeerConnection(boost::asio::io_service& service,
		JobManager *jm) :
	socket(service), jobManager(jm),
	msgReader(socket) {

}

void PeerConnection::start() {
	msgReader.setHandler(boost::bind(&PeerConnection::handleMessageReceived,
				shared_from_this(), _1, _2));

	// All we do is to read the job transfer request
	msgReader.recvMessage();
}

void PeerConnection::handleMessageReceived(std::string &msgString,
		const boost::system::error_code &error) {
	if (!error) {
		// Decode the message and apply the changes
		PeerTransferMessage message;
		message.ParseFromString(msgString);

		const ExecutionPathSet &pathSet = message.path_set();

		std::vector<ExecutionPath*> paths;
		parseExecutionPathSet(pathSet, paths);

		jobManager->importJobs(paths);
	} else {
		CLOUD9_ERROR("Error receiving message from peer");
	}
}

PeerServer::PeerServer(boost::asio::io_service &service, JobManager *jm) :
	acceptor(service, tcp::endpoint(tcp::v4(), LocalPort)), jobManager(jm) {

	startAccept();
}

PeerServer::~PeerServer() {
	// TODO Auto-generated destructor stub
}

void PeerServer::startAccept() {
	CLOUD9_INFO("Listening for peer connections on port" <<
			acceptor.local_endpoint().port());


	PeerConnection::pointer newConn = PeerConnection::create(acceptor.io_service(),
			jobManager);

	acceptor.async_accept(newConn->getSocket(), boost::bind(&PeerServer::handleAccept,
			this, newConn, boost::asio::placeholders::error));
}

void PeerServer::handleAccept(PeerConnection::pointer conn,
		const boost::system::error_code &error) {

	if (!error) {
		conn->start();
		// Go back accepting other connections
		startAccept();
	} else {
		CLOUD9_ERROR("Error accepting peer connection");
	}


}

}

}
