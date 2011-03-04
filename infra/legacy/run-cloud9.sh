#!/bin/bash

export CLOUD9_ROOT=/home/stefan/symbex/state-merging
export KLEE_ROOT=$CLOUD9_ROOT

RUN_WORKER="./run-worker.sh"
RUN_LB="$(dirname $0)/run-lb.sh"

if [ -n "$1" ]
then
	NOWORKERS=$1
else
	echo "You must specify a number of workers."
	exit
fi

if [ -n "$2" ]
then
	EXPNAME=$2
else
	echo "You must specify the name of the experiment."
	exit
fi

if [ -n "$3" ]
then
	PORT_START=$3
else
	echo "You must specify the name of the starting port."
	exit
fi

LBPORT=$PORT_START
WORKER_START=$((PORT_START+1))

echo "Running Cloud9 with $NOWORKERS workers."
echo

for X in $(seq 1 $NOWORKERS)
do
	echo "Launching worker $X..."
	WORKER_PORT=$((WORKER_START+X))
	$RUN_WORKER $WORKER_PORT $LBPORT ${EXPNAME}-worker${X} 2>&1 | tee ${EXPNAME}-output-w${X}.txt &
done

echo "Launching the load balancer..."
$RUN_LB $LBPORT 2>&1 | tee ${EXPNAME}-output-lb.txt &

echo "Now waiting for them to finish..."
for X in $(seq 1 $((NOWORKERS+1)) )
do
	wait %$X
done
