#!/bin/sh

RESULTDIR=result/

if [ ! -d ${RESULTDIR} ];
then
    mkdir ${RESULTDIR}
fi

P=${PBS_NP}

. ../params.sh

SIZE=SIZE_WEAK_${P}


for sz in ${!SIZE} ;
do
    echo ${sz}

    TIMEFILE=${RESULTDIR}/weak_${sz}_${P}
    
    mpirun ${MPIRUN_PARAMS} ./mpi_heat $sz 5 2> ${TIMEFILE}

    process_time_file ${TIMEFILE}
done
