#!/bin/bash

SCRIPT_DIR="$(dirname $0)"
SCRIPT_NAME="$(basename $0)"
DATA_SCRIPT="${SCRIPT_NAME%.*}-data.py"
DATA_FILE=$(python $SCRIPT_DIR/$DATA_SCRIPT)

if [ -z "$DATA_FILE" ]
then
    echo "Could not generate the graph data. Aborting."
    exit 1
fi

GNUPLOT_FILE="${SCRIPT_NAME%.*}.gp"
GNUPLOT_FILE="${GNUPLOT_FILE#gen-}"

gnuplot -e "datafile = \"$DATA_FILE\"" $SCRIPT_DIR/gnuplot/$GNUPLOT_FILE

rm $DATA_FILE