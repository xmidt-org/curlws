#!/bin/bash

base=.
prefix=tests/CMakeFiles
out=out


while getopts b:p:s: flag
do
    case "${flag}" in
        b) base=${OPTARG};;
        p) prefix=${OPTARG};;
        s) out=${OPTARG};;
    esac
done

mkdir -p "${out}"

first=`ls -d ${prefix}/*/ | head -n 1`
all=`ls -d ${prefix}/*/`

gcov-tool merge -o ${out} ${first} ${first}
for dir in ${all}
do
gcov-tool merge -o ${out} ${out} ${dir}
done

gcnos=`find . -type f -name *.gcno`

for gcno in ${gcnos}
do

cp "${gcno}" "${out}/${gcno#*.dir/}"
done

find ${out} -type f -name '*.gcda' -exec gcov -p {} +
