set style line 1 lt rgbcolor "red"
set style line 2 lt rgbcolor "blue"

###################################################################

set xlabel "State multiplicity"
set ylabel "Solving time (seconds)"
set title "Distribution of solving time over state multiplicity"

set output "solver-time-vs-mplicity". ext
set xrange [0:*]
plot datafile using 2:3 index 0 with points ls 1 notitle

set output "solver-time-vs-mplicity-zoom". ext
set xrange [0:50]
replot

###################################################################

set title "Distribution of STP time over state multiplicity"
set xrange [0:*]
set output "stp-time-vs-mplicity". ext
plot datafile using 2:3 index 1 with points ls 2 title "SAT", \
     datafile using 2:3 index 2 with points ls 1 title "UNSAT"

###################################################################

set xlabel "State depth"
set ylabel "State multiplicity"
set title "Distribution of state multiplicity over forking depth"

set output "mplicity-vs-depth". ext
set xrange [0:*]
set logscale y
plot datafile using 1:2 index 5 with boxes title "Average over SAT+UNSAT", \
     datafile using 1:2 index 1 with points ls 2 title "SAT", \
     datafile using 1:2 index 2 with points ls 1 title "UNSAT"
unset logscale y

###################################################################

set xlabel "State depth"
set ylabel "Solving time (seconds)"
set title "Distribution of solving time over state depth"

set output "solver-time-vs-depth". ext
set xrange [0:*]
plot datafile using 1:3 index 1 with points ls 2 title "SAT", \
     datafile using 1:3 index 2 with points ls 1 title "UNSAT"

####################################################################