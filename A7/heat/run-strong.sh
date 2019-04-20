#!/bin/sh

RESULTDIR=result/

if [ ! -d ${RESULTDIR} ];
then
    mkdir ${RESULTDIR}
fi

P=${PBS_NP}

. ../params.sh

for sz in ${SIZE_STRONG};
do
    TIMEFILE=${RESULTDIR}/strong_${sz}_${P}
    
    mpirun ./mpi_heat $sz 5 2> ${TIMEFILE}

    process_time_file ${TIMEFILE}
done
