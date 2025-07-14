#!/bin/bash
rm -rf build
rm *.spv *.h

sleep 6
glslc noise_generator.comp -o noise_generator.spv
glslc shader.vert -o shader.vert.spv
glslc shader.frag -o shader.frag.spv

xxd -i noise_generator.spv > noise_generator.h
xxd -i shader.vert.spv > shader_vert.h
xxd -i shader.frag.spv > shader_frag.h

mkdir build
cd build
cmake ..
make
