#!/bin/bash

# does not account for neg numbers
sci_notation_regex='^[0-9]+([.][0-9]+)?(e[0-9]+|e-[0-9]+)?$'

function test_time {
    # compare 
    if [[ ! $1 =~ $sci_notation_regex ]] ; 
    then
        echo ERROR: time is not on stderr or not formatted properly
        echo
        rm .time
        exit 1
    fi
    # delete tmp file 
    rm .time
}



echo "========================================================="
echo running with simple text
echo "========================================================="
mpirun -np 2 ./wordfreq test.txt 10 2> .time < /dev/null
test_time $(cat .time)
echo
echo "========================================================="
echo compare your output against "wf_simple_answ.txt". Note that the order could be different :
awk '{if ($1 >= 10) print $0;}' < wf_simple_answ.txt
echo
echo "========================================================="

echo
echo "========================================================="
echo running with "big" text
echo "========================================================="
mpirun -np 4 ./wordfreq 16-0.txt 1000 2> .time < /dev/null
test_time $(cat .time)
echo
echo "========================================================="

echo compare your output against "wf_big_answ.txt". Note that the order could be different :
awk '{if ($1 >= 1000) print $0;}' < wf_big_answ.txt

echo "========================================================="
