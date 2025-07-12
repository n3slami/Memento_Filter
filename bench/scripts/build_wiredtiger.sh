#!/bin/bash

export COMPILER=gcc
export CC=$COMPILER
export LD=$COMPILER

cd ../wiredtiger/
mkdir build && cd build 
cmake ../CMakeLists.txt
./configure && make -j8

