/*
 * LBServer.h
 *
 *  Created on: Jan 7, 2010
 *      Author: stefan
 */

#ifndef LBSERVER_H_
#define LBSERVER_H_

#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class LBServer {
private:
	tcp::acceptor acceptor;
public:
	LBServer(boost::asio::io_service &io_service, int port);
	virtual ~LBServer();

	void run();
};

#endif /* LBSERVER_H_ */
