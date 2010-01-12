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

#include "cloud9/Logger.h"

#include <string>
#include <cassert>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <cstring>




using namespace boost::asio::ip;
using namespace cloud9::data;

namespace cloud9 {

class ExecutionPath;

// XXX Debugging
static inline std::string getASCIIMessage(std::string &message) {
	std::string result;
	bool first = true;

	for (std::string::iterator it = message.begin(); it != message.end(); it++) {
		if (!first)
			result.push_back(':');
		else
			first = false;

		result.append(boost::lexical_cast<std::string>((int)(*it)));
	}
	return result;
}

void embedMessageLength(std::string &message);

void sendMessage(tcp::socket &socket, std::string &message);
void recvMessage(tcp::socket &socket, std::string &message);


class AsyncMessageReader {
public:
	typedef boost::function<void (std::string&,
				const boost::system::error_code&)> Handler;
private:
	tcp::socket &socket;

	char *msgData;
	size_t msgSize;

	Handler handler;

	void reset() {
		if (msgData) {
			delete[] msgData;
			msgData = NULL;
			msgSize = 0;
		}
	}

	void handleHeaderRead(const boost::system::error_code &error, size_t size) {
		if (!error) {
			assert(size == sizeof(msgSize));

			msgData = new char[msgSize];

			boost::asio::async_read(socket, boost::asio::buffer(msgData, msgSize),
					boost::bind(&AsyncMessageReader::handleMessageRead,
							this, boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred));
		} else {
			CLOUD9_DEBUG("Header read error: " << error.message());
			std::string message;

			handler(message, error);
			reset();
		}
	}

	void handleMessageRead(const boost::system::error_code &error, size_t size) {
		std::string message;
		if (!error) {
			message = std::string(msgData, msgSize);
			CLOUD9_DEBUG("Received message " << getASCIIMessage(message));
		} else {
			CLOUD9_DEBUG("Message read error: " << error.message());
		}

		handler(message, error);
		reset();
	}
public:
	AsyncMessageReader(tcp::socket &s, Handler h) :
		socket(s), msgData(NULL), msgSize(0), handler(h) { }

	AsyncMessageReader(tcp::socket &s) :
		socket(s), msgData(NULL), msgSize(0) { }

	virtual ~AsyncMessageReader() {}

	void setHandler(Handler h) { handler = h; }
	Handler getHandler() { return handler; }

	void recvMessage() {
		assert(msgData == NULL);

		boost::asio::async_read(socket, boost::asio::buffer(&msgSize, sizeof(msgSize)),
				boost::bind(&AsyncMessageReader::handleHeaderRead,
						this, boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
	}
};

class AsyncMessageWriter {
public:
	typedef boost::function<void (const boost::system::error_code&)> Handler;
private:
	tcp::socket &socket;

	std::string message;

	Handler handler;

	void handleMessageWrite(const boost::system::error_code &error, size_t size) {
		if (!error) {
			CLOUD9_DEBUG("Sent message " << getASCIIMessage(message));
		} else {
			CLOUD9_DEBUG("Message write error" << error.message());
		}

		handler(error);
	}
public:
	AsyncMessageWriter(tcp::socket &s, Handler h) :
		socket(s), handler(h) { }

	AsyncMessageWriter(tcp::socket &s) :
		socket(s) { }

	virtual ~AsyncMessageWriter() { }

	void setHandler(Handler h) { handler = h; }
	Handler getHandler() { return handler; }

	void sendMessage(const std::string &message) {
		size_t msgSize = message.size();
		this->message.clear();
		this->message.append((char*)&msgSize, sizeof(msgSize));
		this->message.append(message.begin(), message.end());

		boost::asio::async_write(socket, boost::asio::buffer(this->message),
				boost::bind(&AsyncMessageWriter::handleMessageWrite,
						this, boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
	}
};


void parseExecutionPathSet(const ExecutionPathSet &ps,
		std::vector<ExecutionPath*> &result);

void serializeExecutionPathSet(const std::vector<ExecutionPath*> &set,
		cloud9::data::ExecutionPathSet &result);

void connectSocket(boost::asio::io_service &service, tcp::socket &socket,
		std::string &address, int port, boost::system::error_code &error);

}


#endif /* PROTOCOLS_H_ */
