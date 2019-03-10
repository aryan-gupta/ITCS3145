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


template <typename T>
using malloc_uptr_t = std::unique_ptr<T, decltype(std::free)*>;

// Just playing around with things.
// interesting: https://stackoverflow.com/questions/6893285/
template <typename T, typename D = typename std::decay<T>::type, typename B = typename std::remove_pointer<D>::type>
malloc_uptr_t<T> malloc_uptr(std::size_t num) {
  if (num != 1) assert(std::is_array<T>::value);

  D ptr = static_cast<D>(std::malloc(sizeof(B) * num));
  malloc_uptr_t<T> uptr{ ptr, &std::free };
  return uptr;
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


namespace parallel {
  int prefixsum_serial(int *start, int *end, int *pr) {
    pr[1] = *start;

    for (++start, ++pr; start != end; ++start, ++pr) {
      pr[1] = *pr + *start;
    }

    return *pr;
  }

  void prefixsum_fix(int *pstart, int* pend, int const *const errors) {
    int offset{  };
    for (int i = 0; i < omp_get_thread_num(); ++i) {
      offset += errors[i];
    }

    while (pstart++ != pend) {
      *pstart += offset;
    }

  }

  void prefixsum(int *const arr, size_t n, int *const pr) {
    malloc_uptr_t<int[]> partials { nullptr, nullptr };
    #pragma omp parallel
    {
      int nbthreads = omp_get_num_threads();
      #pragma omp single
      {
        partials = malloc_uptr<int[]>(nbthreads);
      }

      int gran = n / nbthreads;
      int threadid = omp_get_thread_num();
      int *astart = arr + (gran * threadid);
      int *pstart = pr  + (gran * threadid);
      int *aend   = (threadid == nbthreads - 1)? arr + n : astart + gran; // if we are the last thread then take care of the edge cases
      int *pend   = (threadid == nbthreads - 1)? pr + n : pstart + gran; // if we are the last thread then take care of the edge cases

      partials[threadid] = prefixsum_serial(astart, aend, pstart);
      #pragma omp barrier
      prefixsum_fix(pstart, pend, partials.get());
    }
  }
}


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
  int nbthreads = atoi(argv[2]);
  omp_set_num_threads(nbthreads);


  int * arr = new int [n];
  int * pr = new int [n+1];
  generatePrefixSumData (arr, n);
  // for (int i = 0; i < n; ++i) {
  //   arr[i] = i;
  // }

  float elapse = measure_func(parallel::prefixsum, arr, n, pr);
  std::cerr << elapse << std::endl;

  checkPrefixSumResult(pr, n);

  delete[] arr;
  delete[] pr;

  return 0;
}
