#! /bin/bash

gcc -g -Wall -o test test.c mylib.c my_pthread.c &&
./test
