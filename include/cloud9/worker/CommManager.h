/*
 * CommManager.h
 *
 *  Created on: Jan 9, 2010
 *      Author: stefan
 */

#ifndef COMMMANAGER_H_
#define COMMMANAGER_H_

#include <boost/thread.hpp>

namespace cloud9 {

namespace worker {

class JobManager;

class CommManager {
private:
	class LBCommThread {
	private:
		JobManager *jobManager;
	public:
		LBCommThread(JobManager *jm) : jobManager(jm) { }
		void operator()();
	};

	class PeerCommThread {
	private:
		JobManager *jobManager;
	public:
		PeerCommThread(JobManager *jm) : jobManager(jm) { }
		void operator()();
	};

	PeerCommThread peerCommControl;
	LBCommThread lbCommControl;

	boost::thread peerCommThread;
	boost::thread lbCommThread;

public:
	CommManager(JobManager *jobManager);
	virtual ~CommManager();

	void setup();

	void finalize();
};

}

}

#endif /* COMMMANAGER_H_ */
