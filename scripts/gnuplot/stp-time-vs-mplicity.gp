# Common Setup
set terminal postscript eps enhanced size 6.0 in, 2.0 in
set xlabel "State multiplicity"
set ylabel "STP solving time"

set title "Distribution of STP solving time over state multiplicity"

set output "stp-time-vs-mplicity.eps"

plot datafile using 1:2 with points ps 1 notitle