=================
Installing Cloud9
=================

In order to install Cloud9, you need to perform the following steps (we assume an Ubuntu 10.10 x64 setup):

Building LLVM
-------------

Cloud9 requires LLVM 2.6. You should first download the GCC front-end binaries_, and make sure your $PATH variable points to the bin/ directory of the binaries distribution.

Then download the LLVM 2.6 source_, run `./configure --enable-optimized`, then `make -jN`, where N is the number of available cores on the machine (for speeding up the build process).

Building The Klee C Library
---------------------------

Building Cloud9
---------------

.. _binaries: http://llvm.org/releases/2.6/llvm-gcc-4.2-2.6-x86_64-linux.tar.gz
.. _source: http://llvm.org/releases/2.6/llvm-2.6.tar.gz
