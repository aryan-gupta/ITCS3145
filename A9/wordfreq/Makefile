CXX=mpicxx
CC=mpicxx
LD=mpicxx

HOSTN=$(shell hostname)

CXXMRMPIFLAGS=$(shell if [ "$(HOSTN)" = "mba-i1.uncc.edu" ] ; then echo -I /projects/class/itcs3145_001/mrmpi-7Apr14/src/; else echo -I /usr/include/mrmpi ; fi )

LDMRMPILIBS=$(shell if [ "$(HOSTN)" = "mba-i1.uncc.edu" ] ; then echo /projects/class/itcs3145_001/mrmpi-7Apr14/src/libmrmpi_mpicc.a; else echo -lMapReduceMPI ; fi )

CXXFLAGS=-O2 $(CXXMRMPIFLAGS) -std=c++11
LDFLAGS=$(LDMRMPIFLAGS)
LDLIBS=$(LDMRMPILIBS)


wordfreq: wordfreq.o

bench:
	./queue.sh

test:
	./test.sh

plot:
	./plot.sh

clean:
	-rm *.o
	-rm *.sh.*
	-rm wordfreq

