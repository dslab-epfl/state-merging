/*
 * CommUtils.cpp
 *
 *  Created on: Jan 10, 2010
 *      Author: stefan
 */

#include "cloud9/Protocols.h"
#include "cloud9/ExecutionTree.h"
#include "cloud9/Logger.h"

namespace cloud9 {

void connectSocket(boost::asio::io_service &service, tcp::socket &socket,
		std::string &address, int port,
		boost::system::error_code &error) {

	tcp::resolver resolver(service);

	CLOUD9_DEBUG("Connecting to " << address << ":" << port);
	tcp::resolver::query query(address, boost::lexical_cast<std::string>(port));

	tcp::resolver::iterator it = resolver.resolve(query, error);

	if (!error) {
		error = boost::asio::error::host_not_found;
	} else {
		return;
	}

	tcp::resolver::iterator end;

	while (error && it != end) {
		socket.close();
		socket.connect(*it, error);
		it++;
	}
}

void embedMessageLength(std::string &message) {
	size_t msgSize = message.size();
	message.insert(0, (char*)&msgSize, sizeof(msgSize));
}

void sendMessage(tcp::socket &socket, std::string &message) {
	size_t msgSize = message.size();
	boost::asio::write(socket, boost::asio::buffer(&msgSize, sizeof(msgSize)));
	boost::asio::write(socket, boost::asio::buffer(message));

	CLOUD9_DEBUG("Sent message " << getASCIIMessage(message));
}

void recvMessage(tcp::socket &socket, std::string &message) {
	size_t msgSize = 0;
	char *msgBuff = NULL;

	boost::asio::read(socket, boost::asio::buffer(&msgSize, sizeof(msgSize)));
	assert(msgSize > 0);
	msgBuff = new char[msgSize];

	boost::asio::read(socket, boost::asio::buffer(msgBuff, msgSize));
	message.append(msgBuff, msgSize);

	delete[] msgBuff;

	CLOUD9_DEBUG("Received message " << getASCIIMessage(message));
}

void parseExecutionPathSet(const ExecutionPathSet &ps,
		std::vector<ExecutionPath*> &result) {

	result.clear();

	for (int i = 0; i < ps.path_size(); i++) {
		const ExecutionPathSet_ExecutionPath &p = ps.path(i);

		ExecutionPath *path = new ExecutionPath();

		if (p.has_parent()) {
			path->parent = result[p.parent()];
			path->parentIndex = p.parent_pos();
		}

		const PathData &data = p.data();
		const unsigned char *pathBytes = (const unsigned char*)data.path().c_str();

		for (int j = 0; j < data.length(); j++) {
			path->path.push_back((pathBytes[j / 8] &
					(unsigned char)(1 << (j % 8))) != 0);

			result.push_back(path);
		}
	}
}

void serializeExecutionPathSet(const std::vector<ExecutionPath*> &set,
		cloud9::data::ExecutionPathSet &result) {

	std::map<ExecutionPath*, int> indices;

	for (int i = 0; i < set.size(); i++) {
		ExecutionPath *path = set[i];

		ExecutionPathSet_ExecutionPath *pDest = result.mutable_path()->Add();

		if (set[i]->parent != NULL) {
			assert(indices.find(path->parent) != indices.end());

			pDest->set_parent(indices[path->parent]);
			pDest->set_parent_pos(path->parentIndex);

			PathData *pData = pDest->mutable_data();
			pData->set_length(path->path.size());

			std::string dataStr(path->path.size() / 8 + 1, 0);

			for (int j = 0; j < path->path.size(); j++) {
				if (path->path[j])
					dataStr[j / 8] |= (1 << (j % 8));
				else
					dataStr[j / 8] &= ~(1 << (j % 8));
			}

			pData->set_path(dataStr);
		}

		indices[path] = i;
	}

}

}
