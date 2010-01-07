/*
 * lb.cpp
 *
 *  Created on: Dec 1, 2009
 *      Author: stefan
 */

#include "cloud9/lb/LBServer.h"
#include "cloud9/Logger.h"

#include "llvm/Support/CommandLine.h"

#include <cstdio>
#include <boost/asio.hpp>

using namespace llvm;
using namespace cloud9::lb;

namespace {

cl::opt<int> ServerPort("port",
		cl::desc("The port the load balancing server listens on"),
		cl::init(1234)); // TODO: Move this in a #define

}

int main(int argc, char **argv, char **envp) {
	boost::asio::io_service io_service;

	cl::ParseCommandLineOptions(argc, argv, "Cloud9 load balancer");

	LBServer *server = new LBServer(io_service, ServerPort);

	CLOUD9_INFO("Running message handling loop...");
	io_service.run();
	return 0;
}
