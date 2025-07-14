#!/bin/bash

export COMPILER=gcc
export CC=$COMPILER
export LD=$COMPILER

cd ../wiredtiger/
mkdir build && cd build 
cmake ../ -DBUILD_TYPE=Release -DENABLE_PYTHON=OFF
make -j8

