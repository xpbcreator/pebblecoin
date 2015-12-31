Ubuntu 14.04 build instructions

Install prerequisites:
- sudo apt-get update
- sudo apt-get install cmake ninja-build libboost1.55-all-dev libx11-dev libxext-dev

Static Qt:
- Download latest Qt 4.8 everywhere from https://download.qt.io/official_releases/qt/4.8/
- extract to temporary directory
- cd to qt's root directory
- ./configure -release -opensource -confirm-license -static -no-sql-sqlite -no-opengl -qt-zlib -qt-libpng -qt-libjpeg -no-freetype -no-dbus -no-phonon -no-phonon-backend -no-audio-backend -nomake demos -nomake examples -prefix /usr/local
- make -j4 (change 4 with number of cores)
- sudo make install , or add bin/ folder to PATH

Set up build environment:
- go to the pebblecoin root directory
- mkdir -p build/linux
- cd build/linux
- cmake -D STATIC=1 -D CMAKE_BUILD_TYPE=Release -G Ninja -Wno-dev ../..

Build and run tests:
- ninja test

Once tests pass, build binaries:
- ninja qtwallet daemon simplewallet

Release:
- strip the binaries with "strip"
- no dynamic libraries should be required
