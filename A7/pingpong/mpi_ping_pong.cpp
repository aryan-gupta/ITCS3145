#include <mpi.h>
#include <iostream>

int main (int argc, char* argv[]) {

  if (argc < 2) {
    std::cerr<<"usage: mpirun "<<argv[0]<<" <value>"<<std::endl;
    return -1;
  }

  


  return 0;
}
