#!/bin/bash
git fetch --all
git checkout dev
git rebase master
git stash pop
./build.sh --build-dir build --prefix ~/unstable/darktable_gcc/ --jobs 4 --install
