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
mpirun -np 2 ./knn_mrmpi test.db test.query 10 2> .time < /dev/null
test_time $(cat .time)
echo
echo "========================================================="
echo compare your output against the sequential output: Note that the order could be different. Result could be slightly different also based on how ties are broken:
./knn_seq  test.db test.query 10
echo
echo "========================================================="
