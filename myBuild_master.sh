#!/bin/bash
git stash
git checkout master
git fetch --all
git rebase
git submodule update --init --recursive
#env CMAKE_PREFIX_PATH=/usr/lib/llvm-4.0/
export ASAN_FLAGS="CFLAGS=\"-march=native -Ofast -pipe -fomit-frame-pointer -flto\" "
./build.sh --install --build-dir build --prefix ~/unstable/darktable/ --build-type Release --install
# --enable-camera --enable-colord --enable-graphicsmagick --enable-libsecret --enable-lua --enable-map --enable-nls --enable-opencl --enable-openexr --enable-openmp --enable-unity --enable-webp
