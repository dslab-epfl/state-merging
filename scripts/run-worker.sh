#!/bin/bash

LOCAL_PORT=$1
LBPORT=$2
OUTDIR=$3

setarch $(arch) -R $CLOUD9_ROOT/Release/bin/c9-worker -c9-lb-host localhost -c9-lb-port $LBPORT \
  -c9-local-host localhost -c9-local-port $LOCAL_PORT -output-dir $OUTDIR \
  -c9-use-global-cov --posix-runtime --libc=uclibc --init-env \
  --coverable-modules coreutils.coverable \
  --simplify-sym-indices \
  --output-module --disable-inlining --optimize \
  --use-cex-cache --allow-external-sym-calls --only-output-states-covering-new \
  --max-instruction-time=3600. --max-memory-inhibit=false \
  printf.bc --sym-args 0 2 5

