all: daemon-release

cmake-debug:
	mkdir -p build/debug
	cd build/debug && cmake -D CMAKE_BUILD_TYPE=Debug ../..

test-debug:
	mkdir -p build/debug
	cd build/debug && cmake -D CMAKE_BUILD_TYPE=Debug ../.. && $(MAKE) test

build-daemon-debug: cmake-debug
	cd build/debug && $(MAKE) daemon

build-qtwallet-debug: cmake-debug
	cd build/debug && $(MAKE) qtwallet
  
build-simplewallet-debug: cmake-debug
	cd build/debug && $(MAKE) simplewallet

daemon-debug: build-daemon-debug
qtwallet-debug: build-qtwallet-debug
simplewallet-debug: build-simplewallet-debug


cmake-release:
	mkdir -p build/release
	cd build/release && cmake -D CMAKE_BUILD_TYPE=Release -D STATIC=YES ../..

test-release:
	mkdir -p build/debug
	cd build/release && cmake -D CMAKE_BUILD_TYPE=Release -D STATIC=YES ../.. && $(MAKE) test

build-daemon-release: cmake-release
	cd build/release && $(MAKE) daemon

build-qtwallet-release: cmake-release
	cd build/release && $(MAKE) qtwallet

build-simplewallet-release: cmake-release
	cd build/release && $(MAKE) simplewallet

daemon-release: build-daemon-release
qtwallet-release: build-qtwallet-release
simplewallet-release: build-simplewallet-release

daemon: daemon-release
qtwallet: qtwallet-release
simplewallet: simplewallet-release

clean:
	rm -rf build

tags:
	ctags -R --sort=1 --c++-kinds=+p --fields=+iaS --extra=+q --language-force=C++ src contrib tests/gtest

.PHONY: all cmake-debug build-debug test-debug all-debug cmake-release build-release test-release all-release clean tags
