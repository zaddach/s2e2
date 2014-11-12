#!/bin/sh

git submodule update --init qemu
cd qemu
git submodule update --init dtc
cd ..
git submodule update --init klee
cd klee
git submodule update --init stp
cd ..

