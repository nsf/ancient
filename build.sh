#!/bin/bash

CFLAGS=`llvm-config --cflags`
CXXFLAGS=`llvm-config --cxxflags`
LDFLAGS=`llvm-config --ldflags`
LIBS=`llvm-config --libs bitwriter nativecodegen`

gcc -o tool/lemon tool/lemon.c
ragel main.rl
./tool/lemon grammar.y
gcc -g -c -o main.o $CFLAGS  main.c
gcc -g -c -o parser.o $CFLAGS parser.c
gcc -g -c -o grammar.o $CFLAGS grammar.c
g++ -std=c++0x -g -c -o codegen.o $CXXFLAGS codegen.cpp
echo g++ -std=c++0x -g -o ancient main.o parser.o grammar.o codegen.o $LDFLAGS $LIBS -lreadline
g++ -std=c++0x -g -o ancient main.o parser.o grammar.o codegen.o $LDFLAGS $LIBS -lreadline

