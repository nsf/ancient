#!/bin/bash

CFLAGS=`llvm-config --cflags`
LDFLAGS=`llvm-config --ldflags`
LIBS=`llvm-config --libs all` 

gcc -o tool/lemon tool/lemon.c
ragel main.rl
./tool/lemon grammar.y
gcc -O2 -c -o main.o $CFLAGS  main.c
gcc -O2 -c -o parser.o $CFLAGS parser.c
gcc -O2 -c -o grammar.o $CFLAGS grammar.c
echo g++ -O2 -o ancient main.o parser.o grammar.o $LDFLAGS $LIBS -lreadline
g++ -O2 -o ancient main.o parser.o grammar.o $LDFLAGS $LIBS -lreadline

