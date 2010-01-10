/*
 * CommUtils.h
 *
 *  Created on: Jan 10, 2010
 *      Author: stefan
 */

#ifndef COMMUTILS_H_
#define COMMUTILS_H_

#include <string>


namespace cloud9 {

static inline void embedMessageLength(std::string &message) {
	size_t msgSize = message.size();
	message.insert(0, (char*)&msgSize, sizeof(msgSize));
}

}


#endif /* COMMUTILS_H_ */
