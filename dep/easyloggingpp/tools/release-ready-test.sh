#!/bin/bash

if [ "$1" = "" ];then
    echo "Please specify root directory"
    exit 1
fi

############### --------- Samples ------------ ############

## ------ STL ------- ##
cd $1/samples/STL/
echo "Building STL..."
    echo "Using clang++ compiler"
        sh build_all.sh clang++
        sh run_all.sh
    echo "Using Intel C++ compiler"
        sh build_all.sh icpc
        sh run_all.sh
    echo "Using g++ compiler"
        sh build_all.sh g++
        sh run_all.sh


## ------ boost ------- ##
cd ../../
cd $1/samples/boost
echo "Building boost..."
    echo "Using clang++ compiler"
        sh build_all.sh clang++
        sh run_all.sh
    echo "Using Intel C++ compiler"
        sh build_all.sh icpc
        sh run_all.sh
    echo "Using g++ compiler"
        sh build_all.sh g++
        sh run_all.sh


## ------ gtkmm ------- ##
cd ../../
cd $1/samples/gtkmm
echo "Building gtkmm..."
    echo "Using clang++ compiler"
        sh build_all.sh clang++
        sh run_all.sh
    echo "Using Intel C++ compiler"
        sh build_all.sh icpc
        sh run_all.sh
    echo "Using g++ compiler"
        sh build_all.sh g++
        sh run_all.sh

## ------ gtkmm ------- ##
cd ../../
cd $1/samples/wxWidgets
echo "Building wxWidgets..."
    echo "Using clang++ compiler"
        sh build_all.sh clang++
        sh run_all.sh
    echo "Using Intel C++ compiler"
        sh build_all.sh icpc
        sh run_all.sh
    echo "Using g++ compiler"
        sh build_all.sh g++
        sh run_all.sh

## ------ OpenGL ------- ##
cd ../../
cd $1/samples/OpenGL
echo "Building OpenGL..."
    echo "Using clang++ compiler"
        sh build_all.sh clang++
        sh run_all.sh
    echo "Using Intel C++ compiler"
        sh build_all.sh icpc
        sh run_all.sh
    echo "Using g++ compiler"
        sh build_all.sh g++
        sh run_all.sh

############### --------- Unit Tests ------------ ############

cd ../../
cd $1/test
sh build_and_run.sh
