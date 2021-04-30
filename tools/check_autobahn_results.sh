#!/bin/bash

# SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
# SPDX-License-Identifier: MIT

file=../reports/clients/index.json

while getopts f: flag
do
    case "${flag}" in
        f) file=${OPTARG};;
    esac
done

echo "Report file: ${file}"
echo -e "\nResults"
echo "----------------------------------------"
cat  ${file} | jq -r '.curlws | map_values(if (.behavior == "OK" or .behavior == "INFORMATIONAL") and (.behaviorClose == "OK" or .behaviorClose == "INFORMATIONAL") then "pass" else "----- FAILURE -----" end) | to_entries[] | if "pass" == .value then "\(.key)\t\t\(.value)" else "\u001b[31m\(.key)\t\t\(.value)\u001b[0m" end'
echo "----------------------------------------"
cat  ${file} | jq -r -e '.curlws | map_values(if (.behavior == "OK" or .behavior == "INFORMATIONAL") and (.behaviorClose == "OK" or .behaviorClose == "INFORMATIONAL") then empty else error("\u001b[31mThere was a FAILURE\u001b[m") end) | "All tests passed"'
