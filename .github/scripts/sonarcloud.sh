#!/bin/bash

base=.
src=${base}/src


while getopts b:s: flag
do
    case "${flag}" in
        b) base=${OPTARG};;
        s) src=${OPTARG};;
    esac
done

sources=`find ${src} -type f -name '*.c'`
find ${base} -type f -name '*.gcda' -exec gcov -b -c -p ${sources} {} +
