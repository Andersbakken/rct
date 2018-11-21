rct
===

[![Travis build status](https://travis-ci.org/Andersbakken/rct.svg?branch=master)](https://travis-ci.org/Andersbakken/rct)

A set of c++ tools that provide nicer (more Qt-like) APIs on top of stl classes
released under a BSD license.

## Build Instructions

rct uses the [CMake](https://cmake.org/) build system.

### Unix-Like Systems

On Unix-like systems, building rct is straightforward. Just use cmake and your preferred compiler and build system.

### Windows

Windows support is not complete yet:

- Not all classes are ported to Windows
- Building is only possible with mingw from [MSYS2](http://www.msys2.org/).
- To build the tests, you will also need to build [cppunit](https://freedesktop.org/wiki/Software/cppunit/) yourself.

Once you installed all the prerequisites, you can use cmake to generate "MSYS Makefiles" or "MinGW Makefiles" (both work) to build the library.
