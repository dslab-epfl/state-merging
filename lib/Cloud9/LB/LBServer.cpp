/*
 * LBServer.cpp
 *
 *  Created on: Jan 7, 2010
 *      Author: stefan
 */

#include "cloud9/lb/LBServer.h"
#include "cloud9/lb/WorkerConnection.h"
#include "cloud9/Logger.h"

#include <boost/bind.hpp>

namespace cloud9 {

namespace lb {

LBServer::LBServer(boost::asio::io_service &io_service, int port) :
	acceptor(io_service, tcp::endpoint(tcp::v4(), port)) {

	startAccept();
}

LBServer::~LBServer() {
	// TODO Auto-generated destructor stub
}

void LBServer::startAccept() {
	WorkerConnection *conn = new WorkerConnection(acceptor.io_service());

	CLOUD9_INFO("Listening for connections on port " <<
			acceptor.local_endpoint().port());

	acceptor.async_accept(conn->getSocket(), boost::bind(&LBServer::handleAccept,
			this, conn, boost::asio::placeholders::error));


}

void LBServer::handleAccept(WorkerConnection *conn,
		const boost::system::error_code &error) {
	if (!error) {
		activeConns.insert(conn);

		conn->start();

		// Go back and accept another connection
		startAccept();
	}
}

}

}
