#!/bin/bash

export IMAGE="$(which c9-worker)"

if [ -z "$1" ]
then
    echo "Please specify a destination directory for profile data"
    exit 1
fi

if [ -n "$2" ]
then
    export IMAGE="$2"
fi

export SESS_DIR="$(readlink -f $1)"
export SAMPLING_TIME=60
export ITERATION=0

function reset_oprofile () {
	sudo opcontrol --reset
	sudo opcontrol --deinit
	sudo opcontrol --init
	sudo opcontrol --session-dir="$SESS_DIR" --no-vmlinux --separate=kernel --image="$IMAGE" --callgraph=50
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
