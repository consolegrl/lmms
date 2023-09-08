#!/bin/bash
#
if [ -e build ]; then
	cd build
	make clean
	cd ..
fi
if [ -e build ]; then
	rm -f build
fi
mkdir -p build
cd build
cmake -DCMAKE_INSTALL_PREFIX=../target ..
make -j17 && make install
