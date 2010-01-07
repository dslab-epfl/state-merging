/*
 * WorkerConnection.h
 *
 *  Created on: Jan 7, 2010
 *      Author: stefan
 */

#ifndef WORKERCONNECTION_H_
#define WORKERCONNECTION_H_

#include <boost/asio.hpp>

using boost::asio::ip::tcp;

namespace cloud9 {

namespace lb {

class LBServer;
class Worker;

class WorkerConnection {
	friend class LBServer;
private:
	tcp::socket socket;
	Worker *worker;


	size_t msgSize;
	char *msgData;

	WorkerConnection(boost::asio::io_service &service);

	/*
	 * Starts the asynchronous communication process with the worker
	 */
	void readMessageHeader();

	void readMessageContents(const boost::system::error_code &error, size_t);

	void processMessage(const boost::system::error_code &error, size_t size);

	void finishMessageHandling(const boost::system::error_code &error, size_t);

public:
	virtual ~WorkerConnection();

	tcp::socket &getSocket() { return socket; }
};

}

}

#endif /* WORKERCONNECTION_H_ */
