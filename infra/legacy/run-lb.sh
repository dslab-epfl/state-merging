#!/bin/bash

if [ -n "$1" ]
then
	PORT=$1
else
	PORT=1338
fi

gdb --args $CLOUD9_ROOT/Release/bin/c9-lb -port $PORT

