#!/bin/bash
rm -rf build
rm shader.h
rm hybrid_stress.spv

sleep 6
glslc hybrid_stress.comp -o hybrid_stress.spv

xxd -i hybrid_stress.spv > shader.h
mkdir build
cd build
cmake ..
make
