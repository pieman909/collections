#!/bin/bash
rm -rf build
rm shader.h
rm memory_stress.spv

sleep 6
glslc memory_stress.comp -o memory_stress.spv

xxd -i memory_stress.spv > shader.h
mkdir build
cd build
cmake ..
make
