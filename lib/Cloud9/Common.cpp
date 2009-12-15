/*
 * Common.cpp
 *
 *  Created on: Dec 15, 2009
 *      Author: stefan
 */

#include "cloud9/Common.h"

#include "llvm/Support/CommandLine.h"


using namespace llvm;

std::string InputFile;
LibcType Libc;
bool WithPOSIXRuntime;

namespace {
static cl::opt<std::string, true> InputFileOpt(cl::desc("<input bytecode>"), cl::Positional,
		cl::location(InputFile), cl::init("-"));

static cl::opt<LibcType, true> LibcOpt("c9-libc", cl::desc(
		"Choose libc version (none by default)."), cl::values(
		clEnumValN(NoLibc, "none", "Don't link in a libc"),
		  clEnumValN(UcLibc, "uclibc", "Link in uclibc (adapted for klee)"),
		  clEnumValEnd) , cl::location(Libc), cl::init(NoLibc));

static cl::opt<bool, true> WithPOSIXRuntimeOpt("c9-posix-runtime", cl::desc(
		"Link with POSIX runtime"), cl::location(WithPOSIXRuntime), cl::init(false));

}
