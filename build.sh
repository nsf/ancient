#!/bin/bash

CFLAGS=`llvm-config --cflags`
LDFLAGS=`llvm-config --ldflags`
LIBS=`llvm-config --libs all` 

gcc -o tool/lemon tool/lemon.c
ragel main.rl
./tool/lemon grammar.y
gcc -g -c -o main.o $CFLAGS  main.c
gcc -g -c -o parser.o $CFLAGS parser.c
gcc -g -c -o grammar.o $CFLAGS grammar.c
echo g++ -g -o ancient main.o parser.o grammar.o $LDFLAGS $LIBS -lreadline
g++ -g -o ancient main.o parser.o grammar.o $LDFLAGS $LIBS -lreadline

