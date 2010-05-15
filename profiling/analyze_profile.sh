#!/bin/bash

function sum_data () {
	SUM=0.0
 
	while read LINE
	do 
		SUM=$(echo "$SUM+$LINE" | bc)
	done

	echo $SUM
}

function sum_profile_perc () {
	tr -s " " | cut -d " " -f 2 | sum_data
}

function opreport_sum () {
	SLICE=$1
	FILTER=$2
	opreport --no-header --session-dir=$(pwd)/profile session:slice${SLICE} -lD smart -t 0.001 image:$(which klee) 2>/dev/null | grep $FILTER | sum_profile_perc
}

SLICE_COUNT=60

for SLICE in $(seq 0 $SLICE_COUNT)
do
	MINISAT=$(opreport_sum $SLICE MINISAT)
	BEEV=$(opreport_sum $SLICE BEEV)
	opreport --session-dir=$(pwd)/profile session:slice${SLICE} -lcD smart image:$(which klee) 2>/dev/null | ./gprof2dot.py -f oprofile >slice${SLICE}.dot
	dot -Tpng -o slice${SLICE}.png slice${SLICE}.dot

	echo "Slice $SLICE:" $MINISAT $BEEV $(echo "$MINISAT+$BEEV" | bc)
done
