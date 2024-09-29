<h1>memtag</h1>
<h2>C++ memory tagging proof of concept</h2>

This project serves two purposes, firstly to demonstrate the simplicity of memory tagging (for potential use in GCs), and second to demonstrate the fundamental concepts of heap memory (which nowadays seems to be tantamount to black magic). I was primarily driven in writing this due to constant misunderstanding of how memory works or what a memory leak even is, since it's far too often that you hear that a memory leak is "permanent" or that memory becomes "irretrievable". Unlike similar memory tracking implementations, this doesn't keep track of memory internally via lists or maps, but instead recovers it outright out of the heap.

The program first allocates memory and proceeds to leak it, and then digs it out of the heap. This concept can be extended to a universal "memory freer" or garbage collector. 

## Install
Simply run ninja
```shell
ninja && ./bin/build_memtag_demo_1
```
or
```shell
g++ -std=c++23 src/demo.cpp 
```
glibc and libstdc++ required.

Written only for POSIX/Linux.

Tested with clang/gcc
