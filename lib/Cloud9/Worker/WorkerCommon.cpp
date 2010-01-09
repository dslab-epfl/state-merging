/*
 * Common.cpp
 *
 *  Created on: Dec 15, 2009
 *      Author: stefan
 */

#include "cloud9/worker/WorkerCommon.h"

#include "llvm/Support/CommandLine.h"


using namespace llvm;

std::string InputFile;
LibcType Libc;
bool WithPOSIXRuntime;

JobSelectionType JobSelection;
JobSizingType JobSizing;
JobExplorationType JobExploration;

int MaxJobSize;
int MaxJobDepth;
int MaxJobOperations;

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

static cl::opt<JobSelectionType, true> JobSelectionOpt("c9-jobsel",
		cl::desc("Job selection strategy"), cl::values(
				clEnumValN(RandomSel, "random", "Random selection"),
				clEnumValEnd),
		cl::location(JobSelection), cl::init(RandomSel));

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

}


