#include <mpi.h>
#include <iostream>

int main (int argc, char* argv[]) {

  if (argc < 2) {
    std::cerr<<"usage: mpirun "<<argv[0]<<" <value>"<<std::endl;
    return -1;
  }

	MPI_Init(&argc, &argv);

  int rank{  }, size{  };
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  int num;
  if (rank == 0) {
    num = std::atoi(argv[1]); // read value
    MPI_Send(&num, 1, MPI_INTEGER, 1, 0, MPI_COMM_WORLD); // send to process 1
    MPI_Recv(&num, 1, MPI_INTEGER, 0, 0, MPI_COMM_WORLD, NULL); // get back number (num + 2)
    std::cout << num << std::endl; // print the number
  } else {
    MPI_Recv(&num, 1, MPI_INTEGER, 0, 0, MPI_COMM_WORLD, NULL); // recive number
    num += 2; // add 2
    MPI_Send(&num, 1, MPI_INTEGER, 1, 0, MPI_COMM_WORLD); // send back number
  }

	MPI_Finalize();

  return 0;
}
