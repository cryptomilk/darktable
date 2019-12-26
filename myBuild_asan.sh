#!/bin/bash
git fetch --all
git checkout master
git rebase upstream master
#env CMAKE_PREFIX_PATH=/usr/lib/llvm-4.0/
./build.sh --build-dir build-asan --prefix ~/unstable/darktable-asan/ --jobs 4 --install --asan
