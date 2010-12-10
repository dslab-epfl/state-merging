set terminal png notransparent enhanced size 800, 400

set xlabel "State multiplicity"
set ylabel "STP solving time (seconds)"
set title "Distribution of STP solving time over state multiplicity"

set output "stp-time-vs-mplicity.png"
set xrange [0:*]
plot datafile using 2:3 with points ps 1 notitle

set output "stp-time-vs-mplicity-zoom.png"
set xrange [0:50]
replot

set xlabel "State depth"
set ylabel "State multiplicity"
set title "Distribution of state multiplicity over forking depth"

set output "mplicity-vs-depth.png"
set xrange [0:*]
plot datafile using 1:2 with points ps 1 notitle