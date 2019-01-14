CFLAGS=-O3 -std=c11 -fPIC -Xassembler -mrelax-relocations=no
CXXFLAGS=-O3 -std=c++11 -fPIC -Xassembler -mrelax-relocations=no

LDFLAGS=-fPIE
LDLIBS=libfunctions.a
LD=g++

main: main.o
	$(LD) $(LDFLAGS) main.o $(LDLIBS) -o main

prelim: prelim.o
	$(LD) $(LDFLAGS) prelim.o -o prelim

libfunctions.a: functions.o
	ar rcs libfunctions.a functions.o

.PHONY: plot test bench clean

plot:
	./plot.sh

test: main approx 
	./test.sh

bench: main 
	./bench.sh	

clean:
	-rm main approx prelim
	-rm main.o prelim.o

assignment-numericalintegration.pdf: assignment-numericalintegration.tex
	pdflatex assignment-numericalintegration.tex

assignment-numericalintegration.tgz: assignment-numericalintegration.pdf \
	prelim.cpp queue_prelim.sh run_prelim.sh \
	main.cpp libfunctions.a Makefile \
	approx.cpp test.sh test_cases.txt \
	queue_job.pbs bench.sh run_bench.sh plot.sh .gitignore
	tar zcvf assignment-numericalintegration.tgz \
	assignment-numericalintegration.pdf \
	prelim.cpp queue_prelim.sh run_prelim.sh \
	main.cpp libfunctions.a Makefile \
	approx.cpp test.sh test_cases.txt \
	queue_job.pbs bench.sh run_bench.sh plot.sh .gitignore
