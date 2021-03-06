#!/bin/sh -e

CORES=${nproc}

make --version
qmake --version
cmake --version

git submodule update --init --recursive

cd libs
mkdir -p build_limereport
cd build_limereport
qmake ../LimeReport/limereport.pro -spec linux-g++
make -j$CORES qmake_all
make -j$CORES
qmake ../LimeReport/limereport.pro -spec linux-g++ CONFIG+=debug
make -j$CORES qmake_all
make -j$CORES
cd ../..

cd libs/googletest
mkdir -p build
cd build
cmake -G "Unix Makefiles" ..
make -j$CORES
echo Setup complete
