/*
 * lb.cpp
 *
 *  Created on: Dec 1, 2009
 *      Author: stefan
 */

#include "cloud9/lb/LBServer.h"

#include <cstdio>
#include <boost/asio.hpp>

int main(int argc, char **argv, char **envp) {
	boost::asio::io_service io_service;

	LBServer *server = new LBServer();

	io_service.run();
	return 0;
}
