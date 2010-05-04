/*
 * LBCommon.h
 *
 *  Created on: May 3, 2010
 *      Author: stefan
 */

#ifndef LBCOMMON_H_
#define LBCOMMON_H_

#include "cloud9/Logger.h"
#include "cloud9/lb/Worker.h"

#include <string>
#include <cstdio>

namespace cloud9 {

namespace lb {

static std::string getIDLabel(unsigned int id) {
  char prefix[] = "Worker<   >: ";
  if (id > 0)
    sprintf(prefix, "Worker<%03d>: ", id);

  return std::string(prefix);
}

static std::string getWorkerLabel(const Worker *w) {
  if (!w)
    return getIDLabel(0);
  else
    return getIDLabel(w->getID());
}

#define CLOUD9_ID_ERROR(id, msg)   CLOUD9_ERROR(cloud9::lb::getIDLabel(id) << msg)
#define CLOUD9_ID_INFO(id, msg)    CLOUD9_INFO(cloud9::lb::getIDLabel(id) << msg)
#define CLOUD9_ID_DEBUG(id, msg)   CLOUD9_DEBUG(cloud9::lb::getIDLabel(id) << msg)

#define CLOUD9_WRK_ERROR(w, msg)   CLOUD9_ERROR(cloud9::lb::getWorkerLabel(w) << msg)
#define CLOUD9_WRK_INFO(w, msg)    CLOUD9_INFO(cloud9::lb::getWorkerLabel(w) << msg)
#define CLOUD9_WRK_DEBUG(w, msg)   CLOUD9_DEBUG(cloud9::lb::getWorkerLabel(w) << msg)

}

}


#endif /* LBCOMMON_H_ */
