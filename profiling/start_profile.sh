#!/bin/bash

export SESS_DIR=$(pwd)/profile
export SAMPLING_TIME=60
export ITERATION=5

function reset_oprofile () {
	sudo opcontrol --reset
	sudo opcontrol --deinit
	sudo opcontrol --init
	sudo opcontrol --session-dir=$SESS_DIR --no-vmlinux --separate=kernel --image=$(which klee) --callgraph=50
	sudo opcontrol --start
}

#Initializing oprofile
reset_oprofile

while true
do
	echo "Collecting data for slice $ITERATION"
	sleep $SAMPLING_TIME
	sudo opcontrol --save="slice${ITERATION}"
	sleep 5
	reset_oprofile
	ITERATION=$((ITERATION+1))
done
