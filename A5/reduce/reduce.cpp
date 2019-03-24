#include <omp.h>
#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <tuple>
#include <type_traits>
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif

  void generateReduceData (int* arr, size_t n);

#ifdef __cplusplus
}
#endif


namespace serial {

/// Serially reduces between 2 iterator-like objects
template <typename I>
auto reduce(I start, I end) -> typename std::iterator_traits<I>::value_type {
  typename std::iterator_traits<I>::value_type ans{  };
  for (; start != end; ++start) {
    ans += *start;
  }
  return ans;
}

}


namespace parallel {

/// Parallely reduces between 2 iterator like objects using a granulity (attempt 1 before I read the hint)
template <typename I>
auto reduce(I start, I end) -> typename std::iterator_traits<I>::value_type {
  using val_t = typename std::iterator_traits<I>::value_type;
  constexpr unsigned gran = 100;

  val_t ans{  };

  #pragma omp parallel
  #pragma omp single
  {
    while (start + gran < end) {
      #pragma omp task private(start)
      {
        auto local = serial::reduce(start, start + gran);

        #pragma omp atomic
        ans += local;
      }
      start += gran;
    }

    #pragma omp task private(start)
    {
      auto local = serial::reduce(start, end);

      #pragma omp atomic
      ans += local;
    }
  }

  return ans;
}

} // end namespace parallel


namespace parallel_recurse {

template <typename I>
auto reduce_recurse(I start, I end) -> typename std::iterator_traits<I>::value_type {
  auto dist = std::distance(start, end);
  if (dist == 0) return 0;
  if (dist == 1) return *start;

  I mid = start + (dist / 2);

  int suml, sumr, sum;

  #pragma omp taskgroup
  {
    #pragma omp task
    suml = reduce_recurse(start, mid);
    #pragma omp task
    sumr = reduce_recurse(mid, end);
  }

  #pragma omp task
  sum = suml + sumr;

  return sum;
}

/// Parallely reduces between 2 iterator like objects using recursion
template <typename I>
auto reduce(I start, I end) -> typename std::iterator_traits<I>::value_type {
  typename std::iterator_traits<I>::value_type ans{  };

  #pragma omp parallel
  #pragma omp single
  {
    ans = reduce_recurse(start, end);
  }

  return ans;
}

} // end namespace parallel_recurse

using clk = std::chrono::steady_clock;

/// Measures the exec time of a function and returns the result and time in seconds
/// https://en.cppreference.com/w/cpp/types/result_of See Notes section for why && is needed in result_of
/// @param func The function to call
/// @param args The arguments to pass into the function
/// @return A std::pair containing the time in seconds and the result
template <typename F, typename... A>
auto measure_func(F func, A... args) -> std::pair<float, typename std::result_of<F&&(A&&...)>::type> {
  auto start = clk::now();
  auto result = func( std::forward<A>(args)... );
  auto end = clk::now();
  std::chrono::duration<float> elapse = end - start;
  return { elapse.count(), result };
}


/// Measures the exec time of a function
/// @param func The function to call
/// @param args The arguments to pass into the function
/// @return The execution time in seconds
template<typename F, typename... A,
  typename = typename std::enable_if<std::is_same<typename std::result_of<F&&(A&&...)>::type, void>::value>::type
>
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
    std::cerr<<"usage: "<<argv[0]<<" <n> <nbthreads>"<<std::endl;
    return -1;
  }

  int n = atoi(argv[1]);
  omp_set_num_threads(atoi(argv[2]));

  int * arr = new int [n];

  generateReduceData (arr, atoi(argv[1]));

  float elapse; int answer;
  std::tie(elapse, answer) = measure_func(parallel_recurse::reduce<int*>, arr, arr + n);

  std::cout << answer << std::endl;
  std::cerr << elapse << std::endl;


  delete[] arr;

  return 0;
}
