CXX=mpicxx
CC=mpicc
LD=mpicxx
CXXFLAGS=-std=c++11

mpi_ping_pong:

run: run_1x2
	echo

run_1x2: mpi_ping_pong
	qsub -q mamba -l walltime=00:20:00 -d $(shell pwd) -l nodes=1:ppn=2 ./run.sh

clean:
	rm *.o
	rm *.sh.*
