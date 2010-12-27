set style line 1 lt rgbcolor "red"
set style line 2 lt rgbcolor "blue"
set style line 3 lt rgbcolor "black" lw 2.0

set logscale y

###################################################################

set xlabel "State Multiplicity"
set ylabel "Solving Time (seconds)"
set title "Distribution of Total CS Solving Time Over State Multiplicity"

set logscale x

set output "cs-time-vs-mplicity". ext
set xrange [1:*]
set yrange [:1000]
plot datafile using 2:3 index 0 with points ls 1 notitle, \
     datafile using 1:4 index 6 with histeps ls 3 title "Average"

unset logscale x

###################################################################

set title "Distribution of STP Time Over State Multiplicity"
set xrange [1:*]
set yrange [:1000]
set logscale x

set output "stp-time-vs-mplicity". ext
plot datafile using 2:3 index 1 with points ls 2 title "SAT", \
     datafile using 2:3 index 2 with points ls 1 title "UNSAT", \
     datafile using 1:4 index 4 with histeps ls 3 title "Average over SAT+UNSAT"

unset logscale x

###################################################################

set xlabel "State Depth"
set ylabel "State Multiplicity"
set title "Distribution of State Multiplicity Over Forking Depth"

set output "stp-mplicity-vs-depth". ext
set xrange [0:*]
set yrange [:*]
plot datafile using 1:2 index 1 with points ls 2 title "SAT", \
     datafile using 1:2 index 2 with points ls 1 title "UNSAT", \
     datafile using 1:4 index 5 with histeps ls 3 title "Average over SAT+UNSAT"

###################################################################

set xlabel "State Depth"
set ylabel "Solving Time (seconds)"
set title "Distribution of Solving Time Over State Depth"

set output "stp-time-vs-depth". ext
set xrange [0:*]
set yrange [:1000]
plot datafile using 1:3 index 1 with points ls 2 title "SAT", \
     datafile using 1:3 index 2 with points ls 1 title "UNSAT", \
     datafile using 1:4 index 3 with histeps ls 3 title "Average over SAT+UNSAT"
     

####################################################################