/*
 * Common.cpp
 *
 *  Created on: Dec 15, 2009
 *      Author: stefan
 */

#include "cloud9/worker/WorkerCommon.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/System/Path.h"

#include <cstdlib>
#include <string>


using namespace llvm;
using llvm::sys::Path;

std::string InputFile;
LibcType Libc;
bool WithPOSIXRuntime;

JobSelectionType JobSelection;
JobSizingType JobSizing;
JobExplorationType JobExploration;

int MaxJobSize;
int MaxJobDepth;
int MaxJobOperations;

std::string LBAddress;
int LBPort;

std::string LocalAddress;
int LocalPort;

int RetryConnectTime;
int UpdateTime;

namespace {
static cl::opt<std::string, true> InputFileOpt(cl::desc("<input bytecode>"), cl::Positional,
		cl::location(InputFile), cl::init("-"));

static cl::opt<LibcType, true> LibcOpt("libc", cl::desc(
		"Choose libc version (none by default)."), cl::values(
		clEnumValN(NoLibc, "none", "Don't link in a libc"),
		  clEnumValN(UcLibc, "uclibc", "Link in uclibc (adapted for klee)"),
		  clEnumValEnd) , cl::location(Libc), cl::init(NoLibc));

static cl::opt<bool, true> WithPOSIXRuntimeOpt("posix-runtime", cl::desc(
		"Link with POSIX runtime"), cl::location(WithPOSIXRuntime), cl::init(false));

static cl::opt<JobSelectionType, true> JobSelectionOpt("c9-jobsel",
		cl::desc("Job selection strategy"), cl::values(
				clEnumValN(RandomSel, "random", "Random selection"),
				clEnumValN(RandomPathSel, "random-path", "Random path selection"),
				clEnumValEnd),
		cl::location(JobSelection), cl::init(RandomPathSel));

static cl::opt<JobSizingType, true> JobSizingOpt("c9-jobsizing",
		cl::desc("Job sizing strategy"), cl::values(
				clEnumValN(UnlimitedSize, "unlimited", "Use a single, large job"),
				clEnumValN(FixedSize, "fixed", "Use fix jobs"),
				clEnumValEnd),
		cl::location(JobSizing), cl::init(UnlimitedSize));

static cl::opt<JobExplorationType, true> JobExplorationOpt("c9-jobexpl",
		cl::desc("Job exploration strategy"), cl::values(
				clEnumValN(RandomExpl, "random", "Random exploration"),
				clEnumValEnd),
		cl::location(JobExploration), cl::init(RandomExpl));

static cl::opt<int, true> MaxJobSizeOpt("c9-job-max-size",
		cl::desc("Maximum size for a job in a fixed size strategy"),
		cl::location(MaxJobSize), cl::init(0));

static cl::opt<int, true> MaxJobDepthOpt("c9-job-max-depth",
		cl::desc("Maximum depth for a job in a fixed size strategy"),
		cl::location(MaxJobDepth), cl::init(0));

static cl::opt<int, true> MaxJobOperationsOpt("c9-job-max-ops",
		cl::desc("Maximum count of operations for a job in a fixed size strategy"),
		cl::location(MaxJobOperations), cl::init(0));


static cl::opt<std::string, true> LBAddressOpt("c9-lb-host",
		cl::desc("Host name of the load balancer"),
		cl::location(LBAddress), cl::init("localhost"));

static cl::opt<int, true> LBPortOpt("c9-lb-port",
		cl::desc("Port number of the load balancer"),
		cl::location(LBPort), cl::init(1337));


static cl::opt<std::string, true> LocalAddressOpt("c9-local-host",
		cl::desc("Host name of the local peer server"),
		cl::location(LocalAddress), cl::init("localhost"));

static cl::opt<int, true> LocalPortOpt("c9-local-port",
		cl::desc("Port number of the local peer server"),
		cl::location(LocalPort), cl::init(1234));

static cl::opt<int, true> RetryConnectTimeOpt("c9-lb-connect-retry",
		cl::desc("The time in seconds after the next connection retry"),
		cl::location(RetryConnectTime), cl::init(2));

static cl::opt<int, true> UpdateTimeOpt("c9-lb-update",
		cl::desc("The time in seconds between load balancing updates"),
		cl::location(UpdateTime), cl::init(5));

}

std::string getKleePath() {
	// First look for $KLEE_ROOT, then KLEE_DIR
	char *kleePathName = std::getenv(KLEE_ROOT_VAR);
	Path kleePath;

	if (kleePathName != NULL) {
		// Check whether the path exists
		kleePath = Path(kleePathName);

		if (kleePath.isValid()) {
			// The path exists, so we return it
			kleePath.makeAbsolute();
			return kleePath.toString();
		}
	}

	kleePath = Path(KLEE_DIR);
	kleePath.makeAbsolute();

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
