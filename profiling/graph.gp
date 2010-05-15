set terminal postscript eps enhanced size 6.0 in, 2.0 in
set xlabel "Time (min)"
set ylabel "Time spent (%)"

set xrange [0:80]
set yrange [0:100]

set output "graph.eps"
plot \
	"data.txt" using 1:2 with lines lt 1 lw 2 lc 0 title "Total time in CS", \
	"data.txt" using 1:3 with lines lt 3 lc 3 title "CS for test cases", \
	"data.txt" using 1:4 with lines lt 3 lc 4 title "CS for forks", \
	"data.txt" using 1:5 with lines lt 3 lc 5 title "CS for calls", \
	"data.txt" using 1:6 with lines lt 3 lc 6 title "CS for memops", \
	"data.txt" using 1:7 with lines lt 1 lw 1 lc 1 title "Cache lookup time", \
	"data.txt" using 1:8 with lines lt 1 lw 1.5 lc 2 title "SAT solving"
