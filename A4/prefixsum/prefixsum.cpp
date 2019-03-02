#include <omp.h>
#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <utility>
#include <type_traits>
#include <memory>
#include <cassert>
#include <iterator>

#ifdef __cplusplus
extern "C" {
#endif
  void generatePrefixSumData (int* arr, size_t n);
  void checkPrefixSumResult (int* arr, size_t n);
#ifdef __cplusplus
}
#endif


// prints between 2 iterator like objects to cout
template <typename I, typename V = typename std::iterator_traits<I>::value_type>
void print(I begin, I end) {
  std::copy(begin, end, std::ostream_iterator<V>{ std::cout, " "});
  std::cout << std::endl;
}



namespace serial {
  // decided to change styles. Lets see where this goes
  void prefixsum(int *arr, size_t n, int *pr) {
    pr[0] = 0;

    for (size_t i = 0; i < n; ++i) {
      pr[i + 1] = pr[i] + arr[i];
    }
  }
} // end namespace serial


using clk = std::chrono::high_resolution_clock;
template <typename F, typename... A, typename R = typename std::result_of<F>::type>
auto measure_func(F func, A... args) -> std::pair<float, R> {
  auto start = clk::now();
  R result = func( std::forward<A>(args)... );
  auto end = clk::now();
  std::chrono::duration<float> elapse = end - start;
  return { elapse.count(), std::move(result) };
}

template<typename F, typename... A>
float measure_func(F func, A... args) {
  auto start = clk::now();
  func( std::forward<A>(args)... );
  auto end = clk::now();
  std::chrono::duration<float> elapse = end - start;
  return elapse.count();
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
  
  if (argc < 3) {
    std::cerr<<"Usage: "<<argv[0]<<" <n> <nbthreads>"<<std::endl;
    return -1;
  }

  
  int n = atoi(argv[1]);

  int * arr = new int [n];
  generatePrefixSumData (arr, n);

  int * pr = new int [n+1];

  //insert prefix sum code here

  
  
  checkPrefixSumResult(pr, n);

  delete[] arr;

  return 0;
}
