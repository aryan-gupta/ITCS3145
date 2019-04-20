NODES="1 2 4"
PS_PER_NODE="1 4 8 16"
MPIRUN_PARAMS=""


#the time file should be a single line with a time in seconds.
#some mpi libraries add some debug information. This function strips them from $1
process_time_file () {
    # compare 
    sed -i '/Connect.*/d' $1
    sed -i '/Warning.*/d' $1
}
