/*
 * KleeCommon.cpp
 *
 *  Created on: Apr 6, 2010
 *      Author: stefan
 */

#include "cloud9/worker/KleeCommon.h"

#include "cloud9/Logger.h"
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 9)
#include "llvm/System/Path.h"
#else
#include "llvm/Support/Path.h"
#endif

#include <string>
#include <cstdlib>

using llvm::sys::Path;
using llvm::Twine;

#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)

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

#else

std::string getKleePath() {
	// First look for $KLEE_ROOT, then KLEE_DIR
	char *kleePathName = std::getenv(KLEE_ROOT_VAR);
  Path kleePath;

	if (kleePathName != NULL) {
		CLOUD9_DEBUG("Found KLEE_ROOT variable " << kleePathName);

    if(llvm::sys::path::is_absolute(Twine(kleePathName)))
      kleePath = Path(kleePathName);
    else 
      kleePath = llvm::sys::Path::GetCurrentDirectory();

		// Check whether the path exists
    kleePath.appendSuffix(kleePathName);
		if (kleePath.isValid()) {
			CLOUD9_DEBUG("Using Klee path " << kleePath.str());
			return kleePath.str();
		} else {
			CLOUD9_DEBUG("Cannot use KLEE_ROOT variable. Path is not valid.");
		}
	}

  if(KLEE_DIR !=NULL && llvm::sys::path::is_absolute(Twine(KLEE_DIR)))
    kleePath = Path(KLEE_DIR);
  else{ 
    kleePath = llvm::sys::Path::GetCurrentDirectory();
    kleePath.appendSuffix(KLEE_DIR);
  }
	CLOUD9_DEBUG("Using Klee path " << kleePath.str());
	return kleePath.str();
}

std::string getKleeLibraryPath() {
	std::string kleePathName = getKleePath();

	Path libraryPath(kleePathName);
	libraryPath.appendComponent(RUNTIME_CONFIGURATION);
	libraryPath.appendComponent("lib");

	return libraryPath.str();
}

std::string getUclibcPath() {
	// First look for $KLEE_UCLIBC_ROOT, then KLEE_UCLIBC_DIR
	char *uclibcPathName = std::getenv(KLEE_UCLIBC_ROOT_VAR);
  Path uclibcPath;

	if (uclibcPathName != NULL) {
		CLOUD9_DEBUG("Found KLEE_UCLIBC_ROOT variable " << uclibcPathName);

    if(llvm::sys::path::is_absolute(Twine(uclibcPathName)))
      uclibcPath = Path(uclibcPathName);
    else 
      uclibcPath = llvm::sys::Path::GetCurrentDirectory();

		// Check whether the path exists
    uclibcPath.appendSuffix(uclibcPathName);
		if (uclibcPath.isValid()) {
			CLOUD9_DEBUG("Using Klee path " << uclibcPath.str());
			return uclibcPath.str();
		} else {
			CLOUD9_DEBUG("Cannot use KLEE_UCLIBC_ROOT variable. Path is not valid.");
		}
	}

  if(KLEE_UCLIBC !=NULL && llvm::sys::path::is_absolute(Twine(KLEE_UCLIBC)))
    uclibcPath = Path(KLEE_UCLIBC);
  else{ 
    uclibcPath = llvm::sys::Path::GetCurrentDirectory();
    uclibcPath.appendSuffix(KLEE_UCLIBC);
  }
	CLOUD9_DEBUG("Using Uclibc path " << uclibcPath.str());
	return uclibcPath.str();
}

#endif
