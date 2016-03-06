#!/bin/sh

wget -N https://raw.githubusercontent.com/AlloSphere-Research-Group/AlloProject/master/CMakeLists.txt
wget -N https://raw.githubusercontent.com/AlloSphere-Research-Group/AlloProject/master/run.sh
wget -N https://raw.githubusercontent.com/AlloSphere-Research-Group/AlloProject/master/distclean

chmod 750 run.sh
chmod 750 distclean

mkdir -p cmake_modules
cd cmake_modules
wget -N https://raw.githubusercontent.com/AlloSphere-Research-Group/AlloProject/master/cmake_modules/make_dep.cmake
wget -N https://raw.githubusercontent.com/AlloSphere-Research-Group/AlloProject/master/cmake_modules/CMakeRunTargets.cmake

cd ..
git submodule init
git submodule update --depth 50
git fat init
git fat pull
