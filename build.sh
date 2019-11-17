#!/bin/sh

mkdir -p build; cd build

cmake -G Ninja \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DASAN=OFF \
    -DAUTOTEST=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr \
    ..

ninja
