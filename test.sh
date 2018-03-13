#! /bin/bash

gcc -g -Wall -o test test.c mylib.c
./test
rm test
