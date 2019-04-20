#!/bin/sh

. ../params.sh

mpirun ${MPIRUN_PARAMS} ./mpi_ping_pong 3 
