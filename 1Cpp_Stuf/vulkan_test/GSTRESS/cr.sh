#!/bin/bash
rm -rf build
rm shader.h
rm stress.spv

sleep 6
glslc stress.comp -o stress.spv

xxd -i stress.spv > shader.h
mkdir build
cd build
cmake ..
make
