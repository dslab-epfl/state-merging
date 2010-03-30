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

ExecutionPathPin ExecutionPathSet::getPath(unsigned int index) {
	assert(index < paths.size());

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

ExecutionPathSetPin ExecutionPathSet::parse(std::istream &is) {
	ExecutionPathSet *set = new ExecutionPathSet();

	char crtChar = '\0';

	for (;;) {
		// Move to the first valid path character
		while (!is.eof() && crtChar != '0' && crtChar != '1') {
			is.get(crtChar);
		}
		if (is.eof())
			break;

		// Start building the path
		ExecutionPath *path = new ExecutionPath();
		do {
			path->path.push_back(crtChar - '0');
			is.get(crtChar);
		} while (!is.eof() && (crtChar == '0' || crtChar == '1'));

		set->paths.push_back(path);
	}

	return ExecutionPathSetPin(set);
}

}
