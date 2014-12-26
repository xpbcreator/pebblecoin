all: daemon-release

cmake-debug:
	mkdir -p build/debug
	cd build/debug && cmake -D CMAKE_BUILD_TYPE=Debug ../..

test-debug:
	mkdir -p build/debug
	cd build/debug && cmake -D CMAKE_BUILD_TYPE=Debug -D SMALL_BOULDERHASH=1 ../.. && $(MAKE) test

build-daemon-debug: cmake-debug
	cd build/debug && $(MAKE) daemon

build-simplewallet-debug: cmake-debug
	cd build/debug && $(MAKE) simplewallet

daemon-debug: build-daemon-debug

simplewallet-debug: build-simplewallet-debug


cmake-release:
	mkdir -p build/release
	cd build/release && cmake -D CMAKE_BUILD_TYPE=Release -D STATIC=YES ../..

test-release:
	mkdir -p build/debug
	cd build/release && cmake -D CMAKE_BUILD_TYPE=Release -D STATIC=YES -D SMALL_BOULDERHASH=1 ../.. && $(MAKE) test

build-daemon-release: cmake-release
	cd build/release && $(MAKE) daemon

build-simplewallet-release: cmake-release
	cd build/release && $(MAKE) simplewallet

daemon-release: build-daemon-release
simplewallet-release: build-simplewallet-release

daemon: daemon-release
simplewallet: simplewallet-release

clean:
	rm -rf build

tags:
	ctags -R --sort=1 --c++-kinds=+p --fields=+iaS --extra=+q --language-force=C++ src contrib tests/gtest

.PHONY: all cmake-debug build-debug test-debug all-debug cmake-release build-release test-release all-release clean tags
