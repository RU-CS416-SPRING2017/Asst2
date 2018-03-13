#! /bin/bash

gcc -g -Wall -o test mylib.c
./test
rm test
