# s2e2

##### WARNING #####

This project is for demonstration purposes only!!!!! Some parts of it might be reused, but it is not developed and not planned to be functional. If you are looking for a port of S2E to a newer qemu version, refer to [news2e](https://github.com/eurecom-s3/news2e).

Code base for a version of [S2E][s2e] with more recent [Qemu][qemu] and [Klee][klee] versions.

##### Compiling this project #####

First, you will need a recent CMake (3.2). On Ubuntu 14.04, this can be installed with
```
sudo add-apt-repository ppa:george-edison55/cmake-3.x
sudo apt-get update
sudo apt-get install cmake
```
You will need the following packages installed (Ubuntu 14.04 packages given as example):
```
sudo apt-get install libsigc++-2.0-dev liblua5.1-0-dev llvm-3.4-dev clang-3.4
```
After you have cloned this repository, you need to initialize its submodules
```
git submodule update --init --recursive
```
This will fetch the **qemu**, **klee** and **stp** subprojects.
Next, you need to create a Makefile from the cmake project. For this, change to another directory *$build_dir*.
Run *cmake* here:
```
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=`which clang` \
  -D CMAKE_CXX_COMPILER=`which clang++` $source_dir
```
The CMake variables have the following effect:
  * CMAKE_BUILD_TYPE is used to build either *Debug* or *Release* builds.
  * CMAKE_C_COMPILER sets the C compiler. Currently, only Clang is known to work.
  * CMAKE_CXX_COMPILER sets the C++ compilter. Currently only Clang++ is known to work.
  
Afterwards, run *make* to build the project. Compiling might hang in the qemu build phase, just rerun *make* in
this case (Unfixed bug in the build system).
  
[s2e]: http://s2e.epfl.ch/
[qemu]: http://qemu.org
[klee]: https://klee.github.io/
