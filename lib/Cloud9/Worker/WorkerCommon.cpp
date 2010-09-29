/*
 * Common.cpp
 *
 *  Created on: Dec 15, 2009
 *      Author: stefan
 */

#include "cloud9/worker/WorkerCommon.h"

#include "llvm/Support/CommandLine.h"

#include <cstdlib>
#include <string>


using namespace llvm;

std::string InputFile;
LibcType Libc;
bool WithPOSIXRuntime;

JobSelectionType JobSelection;

bool UseGlobalCoverage;

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
				clEnumValN(CoverageOptimizedSel, "coverage-optimized", "Coverage optimized job selection"),
				clEnumValN(OracleSel, "oracle", "Almighty oracle"),
				clEnumValN(FaultInjSel, "fault-inj", "Fault injection"),
				clEnumValEnd),
		cl::location(JobSelection), cl::init(CoverageOptimizedSel));

static cl::opt<bool, true> UseGlobalCoverageOpt("c9-use-global-cov",
		cl::desc("Use global coverage information in the searcher"),
		cl::location(UseGlobalCoverage), cl::init(false));

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
