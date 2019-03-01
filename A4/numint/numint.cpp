#include <omp.h>
#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
using clk = std::chrono::high_resolution_clock;

using func_t = float (*)(float, int);

#ifdef __cplusplus
extern "C" {
#endif

float f1(float x, int intensity);
float f2(float x, int intensity);
float f3(float x, int intensity);
float f4(float x, int intensity);

#ifdef __cplusplus
}
#endif


float openmp_integrate(func_t functionid, int a, int b, int n, int intensity) {
  float ban = (b - a) / (float)n;
  float ans{  };

  #pragma omp parallel for reduction(+:ans)
  for (int i = 0; i < n; ++i) {
    float x = a + ((float)i + 0.5) * ban;
    ans += functionid(x, intensity);
  }

  return ans * ban;
}



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

  if (argc < 9) {
    std::cerr<<"Usage: "<<argv[0]<<" <functionid> <a> <b> <n> <intensity> <nbthreads> <scheduling> <granularity>"<<std::endl;
    return -1;
  }

  int id = std::atoi(argv[1]);
  int a  = std::atoi(argv[2]);
  int b  = std::atoi(argv[3]);
  int n  = std::atoi(argv[4]);
  int intensity = std::atoi(argv[5]);
  int nbthreads = atoi(argv[6]);
  omp_set_num_threads(nbthreads);
  int gran = atoi(argv[8]);
  switch (argv[7][0]) {
    case 's': omp_set_schedule( omp_sched_static, gran); break;
    case 'd': omp_set_schedule(omp_sched_dynamic, gran); break;
    case 'g': omp_set_schedule( omp_sched_guided, gran); break;
    default : std::cout << "Hold up, what just happened...." << std::endl; return EXIT_FAILURE;
  }


  func_t func = nullptr;
  // depending on the function id. Set the function pointer to the pointer to the function
  switch (id) {
    case 1: func = f1; break;
    case 2: func = f2; break;
    case 3: func = f3; break;
    case 4: func = f4; break;
    default: {
      std::cout << "[E] Ya, you screwed up... have fun" << std::endl;
      return -2;
    }
  }

  auto tStart = clk::now();
  float answer = openmp_integrate(func, a, b, n, intensity);
  auto tEnd = clk::now();

  std::chrono::duration<double> elapse = tEnd - tStart;
  std::cout << answer << std::endl;
  std::cerr << elapse.count() << std::endl;

  return 0;
}
