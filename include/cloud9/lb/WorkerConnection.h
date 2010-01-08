/*
 * WorkerConnection.h
 *
 *  Created on: Jan 7, 2010
 *      Author: stefan
 */

#ifndef WORKERCONNECTION_H_
#define WORKERCONNECTION_H_

#include <boost/asio.hpp>
#include "cloud9/Protocols.h"

using namespace boost::asio::ip;
using namespace cloud9::data;

namespace cloud9 {

namespace lb {

class LBServer;
class Worker;
class LoadBalancer;

class WorkerConnection {
	friend class LBServer;
private:
	tcp::socket socket;
	Worker *worker;


	size_t msgSize;
	char *msgData;

	LoadBalancer *lb;

	WorkerConnection(boost::asio::io_service &service, LoadBalancer *lb);

	/*
	 * Starts the asynchronous communication process with the worker
	 */
	void readMessageHeader();
	void readMessageContents(const boost::system::error_code &error, size_t);
	void processMessage(const boost::system::error_code &error, size_t size);
	void finishMessageHandling(const boost::system::error_code &error, size_t);



	void processNodeSetUpdate(int id, const WorkerReportMessage_NodeSetUpdate &message,
			LBResponseMessage &response);

	void processNodeDataUpdate(int id, const WorkerReportMessage_NodeDataUpdate &message,
			LBResponseMessage &response);

public:
	virtual ~WorkerConnection();

	tcp::socket &getSocket() { return socket; }
};

}

}

#endif /* WORKERCONNECTION_H_ */
