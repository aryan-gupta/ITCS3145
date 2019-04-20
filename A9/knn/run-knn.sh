#!/bin/sh

RESULTDIR=result/

if [ ! -d ${RESULTDIR} ];
then
   mkdir ${RESULTDIR}
fi

. ../params.sh


P=${PBS_NUM_PPN}
NP=$(expr ${PBS_NP} / ${PBS_NUM_PPN})

DBFILE=/projects/class/itcs3145_001/knn_instances/waveform-5000.db
QUERYFILE=/projects/class/itcs3145_001/knn_instances/waveform-5000.query

TIMEFILE=${RESULTDIR}/knn_mrmpi_${NP}_${P}

mpirun ${MPIRUN_PARAMS} ./knn_mrmpi ${DBFILE} ${QUERYFILE} 5 2> ${TIMEFILE}

process_time_file ${TIMEFILE}
