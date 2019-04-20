CXX=mpicxx
CC=mpicc
LD=mpicxx
CFLAGS=-O2 -std=c11 -fPIC -g
CXXFLAGS=-O2 -std=c++11 -fPIC -g
ARCHIVES=libgen.a
LDLIBS=libgen.a

mpi_heat:

test: mpi_heat
	./test.sh

plot: mpi_heat
	./plot.sh

libgen.a: gen_lib.o
	ar rcs libgen.a gen_lib.o

bench: run_strong run_weak

#run_strong: srun_1x1 srun_2x2 srun_3x3 srun_4x4 srun_5x5 srun_6x6
run_strong: srun_1x1 srun_2x2 srun_3x3 srun_4x4 srun_5x5
	echo

srun_1x1: mpi_heat
	qsub -q mamba -l walltime=04:00:00 -d $(shell pwd) -l nodes=1:ppn=1 -l mem=80GB ./run-strong.sh

srun_2x2: mpi_heat
	qsub -q mamba -l walltime=04:00:00 -d $(shell pwd) -l nodes=2:ppn=2 -l mem=80GB ./run-strong.sh

srun_3x3: mpi_heat
	qsub -q mamba -l walltime=04:00:00 -d $(shell pwd) -l nodes=3:ppn=3 -l mem=80GB ./run-strong.sh

srun_4x4: mpi_heat
	qsub -q mamba -l walltime=03:00:00 -d $(shell pwd) -l nodes=4:ppn=4 -l mem=80GB ./run-strong.sh

srun_5x5: mpi_heat
	qsub -q mamba -l walltime=03:00:00 -d $(shell pwd) -l nodes=5:ppn=5 -l mem=80GB ./run-strong.sh

srun_6x6: mpi_heat
	qsub -q mamba -l walltime=03:00:00 -d $(shell pwd) -l nodes=6:ppn=6 -l mem=80GB ./run-strong.sh


#run_weak: wrun_1x1 wrun_2x2 wrun_3x3 wrun_4x4 wrun_5x5 wrun_6x6
run_weak: wrun_1x1 wrun_2x2 wrun_3x3 wrun_4x4 wrun_5x5 
	echo

wrun_1x1: mpi_heat
	qsub -q mamba -l walltime=04:00:00 -d $(shell pwd) -l nodes=1:ppn=1 -l mem=80GB ./run-weak.sh

wrun_2x2: mpi_heat
	qsub -q mamba -l walltime=04:00:00 -d $(shell pwd) -l nodes=2:ppn=2 -l mem=80GB ./run-weak.sh

wrun_3x3: mpi_heat
	qsub -q mamba -l walltime=04:00:00 -d $(shell pwd) -l nodes=3:ppn=3 -l mem=80GB ./run-weak.sh

wrun_4x4: mpi_heat
	qsub -q mamba -l walltime=03:00:00 -d $(shell pwd) -l nodes=4:ppn=4 -l mem=80GB ./run-weak.sh

wrun_5x5: mpi_heat
	qsub -q mamba -l walltime=03:00:00 -d $(shell pwd) -l nodes=5:ppn=5 -l mem=80GB ./run-weak.sh

wrun_6x6: mpi_heat
	qsub -q mamba -l walltime=03:00:00 -d $(shell pwd) -l nodes=6:ppn=6 -l mem=80GB ./run-weak.sh



table_strong:
	./table-strong.sh

table_weak:
	./table-weak.sh

clean:
	rm *.sh.*
