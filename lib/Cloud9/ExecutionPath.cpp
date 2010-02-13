/*
 * ExecutionPath.cpp
 *
 *  Created on: Feb 12, 2010
 *      Author: stefan
 */

#include "cloud9/ExecutionPath.h"

#include "cloud9/ExecutionTree.h"
#include "cloud9/Protocols.h"

#include <string>
#include <vector>
#include <map>

namespace cloud9 {

ExecutionPath *ExecutionPath::getAbsolutePath() {
	ExecutionPath *absPath = new ExecutionPath();
	absPath->parent = NULL;
	absPath->parentIndex = 0;

	ExecutionPath *crtPath = this;
	int crtIndex = crtPath->path.size();

	while (crtPath != NULL) {
		absPath->path.insert(absPath->path.begin(), crtPath->path.begin(),
				crtPath->path.begin() + crtIndex);

		crtIndex = crtPath->parentIndex;
		crtPath = crtPath->parent;
	}

	return absPath;
}

ExecutionPathPin ExecutionPathSet::getPath(int index) {
	assert(index >= 0 && index < paths.size());

	ExecutionPath *path = paths[index]->getAbsolutePath();

	return ExecutionPathPin(path);
}

ExecutionPathSet::ExecutionPathSet() {
	// Do nothing
}


ExecutionPathSet::~ExecutionPathSet() {
	// Delete the paths
	for (iterator it = paths.begin(); it != paths.end(); it++) {
		delete *it;
	}
}

}
