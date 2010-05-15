#!/usr/bin/python

import pydot

SLICE_COUNT = 60

RELEVANT_NODES = {
	"main" : ":klee::main",
	"libc" : ":libc-2.10.1.so::malloc_consolidate",
	"executeInstruction" : ":klee::klee::Executor::executeInstruction(klee::ExecutionState&, klee::KInstruction*)",
	"processTestCase" : ":klee::KleeHandler::processTestCase(klee::ExecutionState const&, char const*, char const*)",
	"fork" : ":klee::klee::Executor::fork(klee::ExecutionState&, klee::ref<klee::Expr>, bool)",
	"executeCall" : ":klee::klee::Executor::executeCall(klee::ExecutionState&, klee::KInstruction*, llvm::Function*, vector<klee::ref<klee::Expr>>&)",
	"executeMemoryOperation" : ":klee::klee::Executor::executeMemoryOperation(klee::ExecutionState&, bool, klee::ref<klee::Expr>, klee::ref<klee::Expr>, klee::KInstruction*)",
	"getAssignment" : ":klee::CexCachingSolver::getAssignment(klee::Query const&, klee::Assignment*&)",
	"lookupAssignment" : ":klee::CexCachingSolver::lookupAssignment(klee::Query const&, set<klee::ref<klee::Expr>, less<klee::ref<klee::Expr>>, allocator<klee::ref<klee::Expr>>>&, klee::Assignment*&)",
	"toSATandSolve" : ":klee::BEEV::BeevMgr::toSATandSolve(MINISAT::Solver&, vector<vector<BEEV::ASTNode>*>&)"
}

def get_percentage(dot, node):
	node_name = '"' + RELEVANT_NODES[node] + '"'
	
	dot_node = dot.get_node(node_name)

	if dot_node is None or type(dot_node) == list:
		return 0.0

	label = dot_node.get("label")

	
	#Split the label after the '\n' character and then get the second value
	perc_str = label.split("\\n")[2]
	
	return float(perc_str[0:-1])

def main():
	print "#SliceNo TotalCS [TestCase Fork Call MemoryOp] [CacheLookup SATSolver] Main LibC ExecInstr"
	for slice in range(0, SLICE_COUNT):
		#print "Processing slice %d" % slice

		# Open the slice file
		dot = pydot.graph_from_dot_file("slice%d.dot" % slice)

		print "%d %.3f  %.3f %.3f %.3f %.3f  %.3f %.3f  %.3f %.3f %.3f" % (slice,
			get_percentage(dot, "getAssignment"),
			get_percentage(dot, "processTestCase"),
			get_percentage(dot, "fork"),
			get_percentage(dot, "executeCall"),
			get_percentage(dot, "executeMemoryOperation"),
			get_percentage(dot, "lookupAssignment"),
			get_percentage(dot, "toSATandSolve"),
			get_percentage(dot, "main"),
			get_percentage(dot, "libc"),
			get_percentage(dot, "executeInstruction") )

if __name__ == "__main__":
	main()

