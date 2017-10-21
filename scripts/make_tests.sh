#!/bin/bash
# Credit: https://stackoverflow.com/questions/10929453/read-a-file-line-by-line-assigning-the-value-to-a-variable

# Arguments:
#   $1 - the unit_tests file location, i.e. scripts/unit_tests
#   $2 - the folder of the logs, i.e. log
#   $3 - the folder to store the tests, i.e. tests

line_num=1
while IFS='' read -r line || [[ -n "$line" ]]; do
	mkdir -p $3/Test${line_num}
	for i in {1..5}
	do
		eval "$line $2/vm$i.log" > $3/Test${line_num}/vm${i}_out.txt
	done
	line_num=$((line_num + 1))
done < "$1"
