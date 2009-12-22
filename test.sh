#!/bin/bash

COREUTILS_DIR=/home/stefan/cloud9/coreutils/obj-llvm/src

gdb --args c9-worker -c9-init-env -c9-libc=uclibc -c9-posix-runtime $COREUTILS_DIR/echo.bc