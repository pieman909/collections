#!/bin/bash
rm -rf build shader_*.h
rm shader*.spv

sleep 6
glslc shader.vert -o shader.vert.spv
glslc shader.frag -o shader.frag.spv


xxd -i shader.vert.spv > shader_vert.h
xxd -i shader.frag.spv > shader_frag.h

mkdir build
cd build
cmake ..
make
