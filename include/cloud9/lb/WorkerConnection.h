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

	WorkerConnection(boost::asio::io_service &service);

	/*
	 * Starts the asynchronous communication process with the worker
	 */
	void start();
public:
	virtual ~WorkerConnection();

	tcp::socket &getSocket() { return socket; }
};

}

}

#endif /* WORKERCONNECTION_H_ */
