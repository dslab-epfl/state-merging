/*
 * WorkerConnection.cpp
 *
 *  Created on: Jan 7, 2010
 *      Author: stefan
 */

#include "cloud9/lb/WorkerConnection.h"
#include "cloud9/Logger.h"

#include <boost/bind.hpp>
#include <string>

namespace cloud9 {

namespace lb {

WorkerConnection::WorkerConnection(boost::asio::io_service &service)
		: socket(service) {
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

		std::string message(msgData, msgSize);

		// Construct the protocol buffer message

	} else {
		CLOUD9_ERROR("Could not read message contents");
	}
}

void WorkerConnection::finishMessageHandling(const boost::system::error_code &error, size_t) {
	if (msgData) {
		delete[] msgData;
		msgData = NULL;
		msgSize = 0;
	}

	// Start over
	readMessageHeader();
}

}

}
