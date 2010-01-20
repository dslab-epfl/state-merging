/*
 * lb.cpp
 *
 *  Created on: Dec 1, 2009
 *      Author: stefan
 */

#include "cloud9/lb/LBServer.h"
#include "cloud9/lb/LoadBalancer.h"
#include "cloud9/Logger.h"
#include "cloud9/Protocols.h"

#include "llvm/Support/CommandLine.h"

#include <cstdio>
#include <boost/asio.hpp>
#include <string>

using namespace llvm;
using namespace cloud9::lb;

namespace {

cl::opt<int> ServerPort("port",
		cl::desc("The port the load balancing server listens on"),
		cl::init(1337)); // TODO: Move this in a #define

cl::opt<int> BalanceRate("balance-rate",
		cl::desc("The rate at which load balancing decisions take place"),
		cl::init(2));

cl::opt<std::string> ServerAddress("address",
		cl::desc("The local address the load balancer listens on"),
		cl::init("localhost"));
}


int main(int argc, char **argv, char **envp) {
	boost::asio::io_service io_service;

	GOOGLE_PROTOBUF_VERIFY_VERSION;

	cl::ParseCommandLineOptions(argc, argv, "Cloud9 load balancer");

	LoadBalancer *lb = new LoadBalancer(BalanceRate);

	LBServer *server = new LBServer(lb, io_service, ServerPort);

	CLOUD9_INFO("Running message handling loop...");
	io_service.run();
	return 0;
}
