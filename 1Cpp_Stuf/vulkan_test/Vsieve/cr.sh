#!/bin/bash

glslc sieve.comp -o sieve.spv
mkdir build
cd build
cmake ..
make
