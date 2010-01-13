/*
 * WorkerConnection.h
 *
 *  Created on: Jan 7, 2010
 *      Author: stefan
 */

#ifndef WORKERCONNECTION_H_
#define WORKERCONNECTION_H_

#include "cloud9/Protocols.h"

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>


using namespace boost::asio::ip;
using namespace cloud9::data;

namespace cloud9 {

namespace lb {

class LBServer;
class Worker;
class LoadBalancer;

class WorkerConnection: public boost::enable_shared_from_this<WorkerConnection> {
private:
	tcp::socket socket;

	LoadBalancer *lb;

	Worker *worker;

	AsyncMessageReader msgReader;
	AsyncMessageWriter msgWriter;

	WorkerConnection(boost::asio::io_service &service, LoadBalancer *lb);


	void handleMessageReceived(std::string &message,
				const boost::system::error_code &error);

	void handleMessageSent(const boost::system::error_code &error);

	void processNodeSetUpdate(int id,
			const WorkerReportMessage_NodeSetUpdate &message);

	void processNodeDataUpdate(int id,
			const WorkerReportMessage_NodeDataUpdate &message);

public:
	typedef boost::shared_ptr<WorkerConnection> pointer;

	static pointer create(boost::asio::io_service &service, LoadBalancer *lb) {
		return pointer(new WorkerConnection(service, lb));
	}

	void start();

	virtual ~WorkerConnection();

	tcp::socket &getSocket() { return socket; }
};

}

}

#endif /* WORKERCONNECTION_H_ */
