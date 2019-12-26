#!/bin/bash
git fetch --all
#git checkout master
git stash
git rebase darktable/master
git stash pop
#env CMAKE_PREFIX_PATH=/usr/lib/llvm-4.0/
./build.sh --build-dir build --prefix ~/unstable/darktable/ -j --install --build-generator "Ninja"
