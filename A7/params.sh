#!/bin/sh

MPIRUN_PARAMS=""

#the time file should be a single line with a time in seconds.
#some mpi libraries add some debug information. This function strips them from $1
process_time_file () {
    # compare 
    sed -i '/Connect.*/d' $1
    sed -i '/Warning.*/d' $1
}



# common
NODES="1 2 3 4 5 6"
PS="2 4 6 8 10 12" # Note that there is no PS=1 by design, to avoid a master only run


# mpi num int params 
NUM_INT_NS="1000000 10000000 100000000"
INTENSITIES="1 10 100 1000"




SIZE_STRONG="11400 36000 72000"

SIZE_WEAK_1="7900 11400 15600"
SIZE_WEAK_4="15800 22800 31200"
SIZE_WEAK_9="23700 34200 46800"
SIZE_WEAK_16="31600 45600 62400"
SIZE_WEAK_25="39500 57000 78000"

SIZE_BIG="216000"

