/*
 * Protocols.h
 *
 *  Created on: Jan 7, 2010
 *      Author: stefan
 */

#ifndef PROTOCOLS_H_
#define PROTOCOLS_H_

// TODO Fix this hack in the Makefile
#include "../../lib/Cloud9/Cloud9Data.pb.h"

#include <string>
#include <cassert>
#include <boost/asio.hpp>


using namespace boost::asio::ip;
using namespace cloud9::data;

namespace cloud9 {

class ExecutionPath;

static inline void embedMessageLength(std::string &message) {
	size_t msgSize = message.size();
	message.insert(0, (char*)&msgSize, sizeof(msgSize));
}

static inline void sendMessage(tcp::socket &socket, std::string &message) {
	size_t msgSize = message.size();
	boost::asio::write(socket, boost::asio::buffer(&msgSize, sizeof(msgSize)));
	boost::asio::write(socket, boost::asio::buffer(message));
}

void recvMessage(tcp::socket &socket, std::string &message);

void parseExecutionPathSet(const ExecutionPathSet &ps,
		std::vector<ExecutionPath*> &result);

void serializeExecutionPathSet(const std::vector<ExecutionPath*> &set,
		cloud9::data::ExecutionPathSet &result);

void connectSocket(boost::asio::io_service &service, tcp::socket &socket,
		std::string &address, int port, boost::system::error_code &error);

}


#endif /* PROTOCOLS_H_ */
