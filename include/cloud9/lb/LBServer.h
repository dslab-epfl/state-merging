/*
 * LBServer.h
 *
 *  Created on: Jan 7, 2010
 *      Author: stefan
 */

#ifndef LBSERVER_H_
#define LBSERVER_H_

#include <boost/asio.hpp>
#include <set>

using boost::asio::ip::tcp;

namespace cloud9 {

namespace lb {

class WorkerConnection;

class LBServer {
private:
	tcp::acceptor acceptor;

	std::set<WorkerConnection*> activeConns;

	void startAccept();

	void handleAccept(WorkerConnection *conn,
			const boost::system::error_code &code);
public:
	LBServer(boost::asio::io_service &io_service, int port);
	virtual ~LBServer();

	void run();
};

}

}

#endif /* LBSERVER_H_ */
