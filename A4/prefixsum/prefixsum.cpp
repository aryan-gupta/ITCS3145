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

#ifdef __cpp_concepts
template <typename T>
concept bool ForwardIterator() {
  return requires(T a) {
    { ++a } -> T;
    { a++ } -> T;
    { *a  } -> typename std::iterator_traits<T>::value_type;
  };
}
#endif

/// prints between 2 iterator like objects to cout
#ifdef __cpp_concepts
template <ForwardIterator I>
#else
template <typename I>
#endif
void print(I begin, I end) {
  using ValueType = typename std::iterator_traits<I>::value_type;
  std::copy(begin, end, std::ostream_iterator<ValueType>{ std::cout, " "});
  std::cout << std::endl;
}


// Just playing around with things.
// interesting: https://stackoverflow.com/questions/6893285/
// https://en.cppreference.com/w/cpp/types/disjunction "C++17"... ughhhhhh
/// This alias and function creates a unique pointer but does not use new/delete. It uses
/// malloc and free.
template <typename T>
using malloc_uptr_t = std::unique_ptr<T, decltype(std::free)*>;

template<typename T>
malloc_uptr_t<T> get_malloc_uptr(size_t num = 1) {
  using BaseType = typename std::remove_extent<T>::type;
  using PointerType = typename std::add_pointer<BaseType>::type;

  if (num != 1) assert(std::is_array<T>::value); // if num is 0 then it just returns nullptr

  auto ptr = static_cast<PointerType>(std::malloc(sizeof(BaseType) * num));
  malloc_uptr_t<T> uptr{ ptr, &std::free };
  return uptr;
}


namespace serial {
  /// Serial version of the prefixsum code
  /// @param arr The original array to conduct prefix sum on
  /// @param n   The size of \p arr
  /// @param pr  The array to store the result (must be of size n + 1)
  void prefixsum(int *arr, size_t n, int *pr) {
    pr[0] = 0;

    for (size_t i = 0; i < n; ++i) {
      pr[i + 1] = pr[i] + arr[i];
    }
  }
} // end namespace serial


namespace parallel {
  /// Conducts prefixsum between 2 int iterators
  /// @param start The start of the array
  /// @param end   The end of the array
  /// @param pr    The start of the array where to store the result
  int prefixsum_serial(int *start, int *end, int *pr) {
    pr[1] = *start;

    for (++start, ++pr; start != end; ++start, ++pr) {
      pr[1] = *pr + *start;
    }

    return *pr;
  }

  /// Fixes prefixsum array by combining the results of the other threads
  /// @param pstart The start of the prefixsum array to fix
  /// @param pend   The end of the prefixsum array
  /// @param errors An array of the other thread's offset/errors
  void prefixsum_fix(int *pstart, int* pend, int const *const errors) {
    int offset{  };
    for (int i = 0; i < omp_get_thread_num(); ++i) {
      offset += errors[i];
    }

    while (pstart++ != pend) {
      *pstart += offset;
    }

  }

  /// Calculates prefix sum on an \p arr and stores it in \p pr
  /// @param arr The original array to conduct prefix sum on
  /// @param n   The size of \p arr
  /// @param pr  The array to store the result (must be of size n + 1)
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
/// Measures the exec time of a function and returns the result and time in seconds
/// @param func The function to call
/// @param args The arguments to pass into the function
/// @return A std::pair containing the time in seconds and the result
template <typename F, typename... A>
auto measure_func(F func, A... args) -> std::pair<float, typename std::result_of<F>::type> {
  auto start = clk::now();
  auto result = func( std::forward<A>(args)... );
  auto end = clk::now();
  std::chrono::duration<float> elapse = end - start;
  return { elapse.count(), std::move(result) };
}


/// Measures the exec time of a function
/// @param func The function to call
/// @param args The arguments to pass into the function
/// @return The execution time in seconds
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
