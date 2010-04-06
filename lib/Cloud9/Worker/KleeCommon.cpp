/*
 * KleeCommon.cpp
 *
 *  Created on: Apr 6, 2010
 *      Author: stefan
 */

#include "cloud9/worker/KleeCommon.h"

#include "cloud9/Logger.h"
#include "llvm/System/Path.h"

#include <string>
#include <cstdlib>

using llvm::sys::Path;

std::string getKleePath() {
	// First look for $KLEE_ROOT, then KLEE_DIR
	char *kleePathName = std::getenv(KLEE_ROOT_VAR);
	Path kleePath;

	if (kleePathName != NULL) {
		// Check whether the path exists
		kleePath = Path(kleePathName);
		CLOUD9_DEBUG("Found KLEE_ROOT variable " << kleePath.toString());

		if (kleePath.isValid()) {
			// The path exists, so we return it
			kleePath.makeAbsolute();
			CLOUD9_DEBUG("Using Klee path " << kleePath.toString());
			return kleePath.toString();
		} else {
			CLOUD9_DEBUG("Cannot use KLEE_ROOT variable. Path is not valid.");
		}
	}

	kleePath = Path(KLEE_DIR);
	kleePath.makeAbsolute();
	CLOUD9_DEBUG("Using Klee path " << kleePath.toString());
	return kleePath.toString();
}

std::string getKleeLibraryPath() {
	std::string kleePathName = getKleePath();

	Path libraryPath(kleePathName);
	libraryPath.appendComponent(RUNTIME_CONFIGURATION);
	libraryPath.appendComponent("lib");

	return libraryPath.toString();
}

std::string getUclibcPath() {
	char *uclibcPathName = std::getenv(KLEE_UCLIBC_ROOT_VAR);
	Path uclibcPath;

	if (uclibcPathName != NULL) {
		uclibcPath = Path(uclibcPathName);

		if (uclibcPath.isValid()) {
			uclibcPath.makeAbsolute();
			return uclibcPath.toString();
		}
	}

	uclibcPath = Path(KLEE_UCLIBC);
	uclibcPath.makeAbsolute();

	return uclibcPath.toString();
}
