#!/bin/sh

RESULTDIR=result/

SIZE="11400 36000 72000"

for sz in $SIZE ;
do

    echo ${sz}
    #    for i in 1 4 9 16 25 36;
    for i in 1 4 9 16 25;
    do
        echo $i $(cat ${RESULTDIR}/strong_${sz}_${i})
#        echo $i $(cat ${RESULTDIR}/${sz}_${i})
    done

    echo "------------------------"
  
done

