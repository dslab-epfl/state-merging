set terminal postscript eps enhanced size 6.0 in, 2.0 in
set xlabel "Time (min)"
set ylabel "Time spent (%)"

set xrange [0:80]
set yrange [0:100]

set style data line

set output "graph_stack_general.eps"
plot \
	"data.txt" using ($1):($9+$10) title "Malloc Consolidation" with filledcurves y1=0.0 lt rgb "#dd0000", \
	"data.txt" using ($1):($9) title "Regular Klee\nExecution" with filledcurves y1=0.0 lt rgb "#00dd00"

set output "graph_stack_klee.eps"
plot \
	"data.txt" using ($1):($9+$10) title "Malloc Consolidation" with filledcurves y1=0.0 lt rgb "#999999", \
	"data.txt" using ($1):($9) title "Constraint Solving" with filledcurves y1=0.0 lt rgb "#dd0000", \
	"data.txt" using ($1):($9-$2) title "Other Klee Execution" with filledcurves y1=0.0 lt rgb "#dddd00", \
	"data.txt" using ($1):($11-$2) title "Instruction Execution" with filledcurves y1=0.0 lt rgb "#00dd00"
	
set output "graph_stack_cs.eps"
plot \
	"data.txt" using ($1):($9+$10) title "Other Klee Exec. + Malloc Cons." with filledcurves y1=0.0 lt rgb "#999999", \
	"data.txt" using ($1):($2) title "STP Specific Functionality" with filledcurves y1=0.0 lt rgb "#dd0000", \
	"data.txt" using ($1):($7+$8) title "SAT Solving" with filledcurves y1=0.0 lt rgb "#00dd00", \
	"data.txt" using ($1):($7) title "Constraint Cache Search" with filledcurves y1=0.0 lt rgb "#0000dd"

set output "graph_overview.eps"
plot \
        "data.txt" using 1:2 with lines lt 1 lw 2 lc 0 title "Total time in CS", \
        "data.txt" using 1:3 with lines lt 3 lc 3 title "CS for test cases", \
        "data.txt" using 1:4 with lines lt 3 lc 4 title "CS for forks", \
        "data.txt" using 1:5 with lines lt 3 lc 5 title "CS for calls", \
        "data.txt" using 1:6 with lines lt 3 lc 6 title "CS for memops", \
        "data.txt" using 1:7 with lines lt 1 lw 1 lc 1 title "Cache lookup time", \
        "data.txt" using 1:8 with lines lt 1 lw 1.5 lc 2 title "SAT solving"