#!/bin/bash

if [ -z "$1" ]
then
    echo "You need to specify the session directory"
    exit 1
fi

export SESSION_DIR="$1"
export IMAGE="$(which c9-worker)"

if [ -n "$2" ]
then
    export IMAGE="$2"
fi


SLICE_COUNT=60
GPROF2DOT="$(dirname $0)/helpers/gprof2dot.py"

for SLICE in $(seq 0 $SLICE_COUNT)
do
	if [ ! -f slice${SLICE}.dot ]
	then
		opreport --session-dir=$SESSION_DIR session:slice${SLICE} -lcD smart image:$IMAGE 2>/dev/null | $GPROF2DOT -f oprofile >slice${SLICE}.dot
	fi

	dot -Tpdf -o slice${SLICE}.pdf slice${SLICE}.dot

	echo "Slice $SLICE: Done."
done
