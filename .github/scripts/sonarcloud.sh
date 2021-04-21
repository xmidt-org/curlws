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

pushd "${dir}"
gcnos=`find . -type f -name *.gcno`
popd

for gcno in ${gcnos}
do
mkdir -p "${gcno}"
cp ${dir}/${gcno} ${out}/${gcno}
done

done

find ${out} -type f -name '*.gcda' -exec gcov -abcp {} +
