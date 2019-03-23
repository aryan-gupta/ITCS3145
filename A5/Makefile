CFLAGS=-O3 -std=c11 -fPIC -g -fopenmp -Xassembler -mrelax-relocations=no
CXXFLAGS=-O3 -std=c++11 -fPIC -g -fopenmp -Xassembler -mrelax-relocations=no
LDFLAGS=-fopenmp
ARCHIVES=libgen.a 
LD=g++


all: assignment-openmp-advanced.tgz


# archives
libgen.a: gen_lib.o
	ar rcs libgen.a gen_lib.o

assignment-openmp-advanced.pdf: assignment-openmp-advanced.tex
	pdflatex assignment-openmp-advanced.tex

assignment-openmp-advanced.tgz: .gitignore Makefile approx.cpp \
                            libgen.a \
                            sequential reduce mergesort bubblesort lcs \
                            params.sh \
                            assignment-openmp-advanced.pdf
	tar zcvf assignment-openmp-advanced.tgz \
                            .gitignore libgen.a \
                            Makefile approx.cpp \
                            sequential reduce mergesort bubblesort lcs \
                            .gitignore params.sh \
                            assignment-openmp-advanced.pdf
