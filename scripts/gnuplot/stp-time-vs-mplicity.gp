set terminal postscript eps enhanced size 4.0 in, 2.0 in

set xlabel "State multiplicity"
set ylabel "STP solving time (seconds)"

set title "Distribution of STP solving time over state multiplicity"

set output "stp-time-vs-mplicity.eps"
set xrange [0:*]
plot datafile using 1:2 with points ps 1 notitle

set output "stp-time-vs-mplicity-zoom.eps"
set xrange [0:50]
replot

set terminal png notransparent enhanced size 800, 400

set output "stp-time-vs-mplicity.png"
set xrange [0:*]
replot

set output "stp-time-vs-mplicity-zoom.png"
set xrange [0:50]
replot