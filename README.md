## Building Pebblecoin

### On *nix:

Dependencies: 
* GCC 4.7.3 or later ( http://gcc.gnu.org/ ), 
* CMake 2.8.6 or later ( http://www.cmake.org/ ), and 
* Boost 1.53 or later (except 1.54, more details here: http://goo.gl/RrCFmA) ( http://www.boost.org/ ).

Alternatively, it may be possible to install them using a package manager.

To build, change to a directory where this file is located, and run `make`. The resulting executables can be found in build/release/src. Run `make qtwallet` to make the qtwallet.

Advanced options:
* Parallel build: run `make -j<number of threads>` instead of `make`.
* Debug build: run `make daemon-debug` or `make qtwallet-debug`.
* Test suite: run `make test-release` to run tests in addition to building. Running `make test-debug` will do the same to the debug version.
* Building with Clang: it may be possible to use Clang instead of GCC, but this may not work everywhere. To build, run `export CC=clang CXX=clang++` before running `make`.

### On Windows: 
* See [doc/BUILD_WINDOWS.txt](https://github.com/xpbcreator/pebblecoin/blob/master/doc/BUILD_WINDOWS.txt).
