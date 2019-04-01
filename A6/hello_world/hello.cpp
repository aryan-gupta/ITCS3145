#include <mpi.h>
#include <unistd.h>
#include <iostream>

#include <limits.h>
#include <iostream>

int main(int argc, char*argv[]) {

	int rank = 0, size = 0;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	char str[HOST_NAME_MAX + 1];
	int ret = gethostname(str, HOST_NAME_MAX + 1);

	std::cout << "I am process "<< rank << " out of " << size << " . I am running on " << str << "." << std::endl;

	MPI_Finalize();
	return 0;
}
