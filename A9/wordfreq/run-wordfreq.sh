#!/bin/sh

RESULTDIR=result/

if [ ! -d ${RESULTDIR} ];
then
   mkdir ${RESULTDIR}
fi

. ../params.sh


P=${PBS_NUM_PPN}
NP=$(expr ${PBS_NP} / ${PBS_NUM_PPN})

INSTFILE=/projects/class/itcs3145_001/texts/alltexts.txt

TIMEFILE=${RESULTDIR}/wordfreq_${NP}_${P}

mpirun ${MPIRUN_PARAMS} ./wordfreq ${INSTFILE} 100000 2> ${TIMEFILE}

process_time_file ${TIMEFILE}
