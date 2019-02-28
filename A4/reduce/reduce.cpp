#include <omp.h>
#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <chrono>
using clk = std::chrono::high_resolution_clock;


#ifdef __cplusplus
extern "C" {
#endif
  void generateReduceData (int* arr, size_t n);
#ifdef __cplusplus
}
#endif


int main (int argc, char* argv[]) {
  //forces openmp to create the threads beforehand
#pragma omp parallel
  {
    int fd = open (argv[0], O_RDONLY);
    if (fd != -1) {
      close (fd);
    }
    else {
      std::cerr<<"something is amiss"<<std::endl;
    }
  }

  if (argc < 5) {
    std::cerr<<"Usage: "<<argv[0]<<" <n> <nbthreads> <scheduling> <granularity>"<<std::endl;
    return -1;
  }

  int n = atoi(argv[1]);
  int nbthreads = atoi(argv[2]);
  omp_set_num_threads(nbthreads);

  int gran = atoi(argv[4]);
  switch (argv[3][0]) {
    case 's': omp_set_schedule( omp_sched_static, gran); break;
    case 'd': omp_set_schedule(omp_sched_dynamic, gran); break;
    case 'g': omp_set_schedule( omp_sched_guided, gran); break;
    default : std::cout << "Hold up, what just happened...." << std::endl; return EXIT_FAILURE;
  }

  int * arr = new int [n];

  generateReduceData (arr, atoi(argv[1]));

  auto tStart = clk::now();
  int sum{  };

  #pragma omp parallel
  {
    int psum{  };

    #pragma omp for
    for (size_t i = 0; i < n; ++i) {
      psum += arr[i];
    }

    #pragma omp critical
    sum += psum;
  }

  auto tEnd = clk::now();
  std::chrono::duration<double> elapse = tEnd - tStart;

  std::cout << sum << std::endl;
  std::cerr << elapse.count() << std::endl;

  delete[] arr;

  return 0;
}
