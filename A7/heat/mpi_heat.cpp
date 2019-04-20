#include <mpi.h>
#include <math.h>
#include <iostream>


#ifdef __cplusplus
extern "C" {
#endif
  double generate2DHeat(long n, long global_i, long global_j);

  int check2DHeat(long n, long global_i, long global_j, double v, long k); //this function return 1 on correct. 0 on incorrect. Note that it may return 1 on incorrect. But a return of 0 means it is definitely incorrect
#ifdef __cplusplus
}
#endif



int main(int argc, char* argv[]) {

  if (argc < 3) {
    std::cerr<<"usage: mpirun "<<argv[0]<<" <N> <K>"<<std::endl;
    return -1;
  }

  // declare and init command line params
  long N, K;
  N = atol(argv[1]);
  K = atol(argv[2]);

  // use double for heat 2d 

  // write code here


  return 0;
}

