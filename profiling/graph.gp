set terminal postscript eps enhanced size 6.0 in, 2.0 in
set xlabel "Time (min)"
set ylabel "Time spent (%)"

set xrange [0:80]
set yrange [0:100]

set style data line

set output "graph_general.eps"
plot \
	"data.txt" using ($1):(100.0) title "Other" with filledcurves y1=0.0 lt rgb "#dddddd", \
	"data.txt" using ($1):($9+$10) title "LibC Internal" with filledcurves y1=0.0 lt rgb "#dd0000", \
	"data.txt" using ($1):($9) title "Regular Klee\nExecution" with filledcurves y1=0.0 lt rgb "#00dd00"

set output "graph_klee.eps"
plot \
	"data.txt" using ($1):(100.0) title "Other" with filledcurves y1=0.0 lt rgb "#dddddd", \
	"data.txt" using ($1):($9) title "Other Klee Execution" with filledcurves y1=0.0 lt rgb "#dddd00", \
	"data.txt" using ($1):($11) title "Instruction Execution" with filledcurves y1=0.0 lt rgb "#00dd00", \
	"data.txt" using ($1):($2) title "Constraint Solving" with filledcurves y1=0.0 lt rgb "#dd0000"
	
set output "graph_cs.eps"
plot \
	"data.txt" using ($1):(100.0) title "Other" with filledcurves y1=0.0 lt rgb "#dddddd", \
	"data.txt" using ($1):($9) title "Other Klee execution" with filledcurves y1=0.0 lt rgb "#999999", \
	"data.txt" using ($1):($2) title "STP Specific functionality" with filledcurves y1=0.0 lt rgb "#dd0000", \
	"data.txt" using ($1):($7+$8) title "SAT Solving" with filledcurves y1=0.0 lt rgb "#00dd00", \
	"data.txt" using ($1):($7) title "Constraint cache search" with filledcurves y1=0.0 lt rgb "#0000dd"

	