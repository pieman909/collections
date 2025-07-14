#!/bin/bash

g++ -std=c++17 -O3 -DNDEBUG -o vulkan_primality_tester vulkan_primality_tester.cpp -lvulkan -lgmp

./vulkan_primality_tester 0

glslc miller_rabin.comp -o miller_rabin.spv

./vulkan_primality_tester 5
