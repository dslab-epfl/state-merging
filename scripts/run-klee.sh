#!/bin/bash

setarch $(arch) -R $CLOUD9_ROOT/Release/bin/klee \
  -output-dir klee \
  --posix-runtime --libc=uclibc --init-env \
  --coverable-modules coreutils.coverable \
  --simplify-sym-indices --all-external-warnings \
  --output-module --disable-inlining --optimize \
  --use-cex-cache --allow-external-sym-calls --only-output-states-covering-new \
  --max-instruction-time=3600. --max-memory-inhibit=false \
  --use-random-path \
  --use-interleaved-covnew-NURS --use-batching-search \
  printf.bc --sym-args 0 2 5

