/*
 * LBServer.cpp
 *
 *  Created on: Jan 7, 2010
 *      Author: stefan
 */

#include "LBServer.h"

LBServer::LBServer(boost::asio::io_service &io_service, int port) :
	acceptor(io_service, tcp::endpoint(tcp::v4(), port)) {


}

LBServer::~LBServer() {
	// TODO Auto-generated destructor stub
}
