/*
 * WorkerConnection.cpp
 *
 *  Created on: Jan 7, 2010
 *      Author: stefan
 */

#include "cloud9/lb/WorkerConnection.h"

namespace cloud9 {

namespace lb {

WorkerConnection::WorkerConnection(boost::asio::io_service &service)
		: socket(service) {
	// TODO Auto-generated constructor stub

}

WorkerConnection::~WorkerConnection() {
	// TODO Auto-generated destructor stub
}

void WorkerConnection::start() {

}

}

}
