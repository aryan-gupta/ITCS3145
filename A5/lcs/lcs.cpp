#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

#include <omp.h>

#include <chrono>
#include <type_traits>
#include <tuple>

#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

  void generateLCS(char* X, int m, char* Y, int n);
  void checkLCS(char* X, int m, char* Y, int n, int result);

#ifdef __cplusplus
}
#endif


namespace detail {
  template <typename I1, typename I2, typename O>
  size_t longest_common_subsequence(I1 begin1, I1 end1, I2 begin2, I2 end2, O&& op) {
    if (begin1 == end1 or begin2 == end2) return 0;

    auto last1 = std::next(end1, -1);
    auto last2 = std::next(end2, -1);

    if (op(*last1, *last2)) {
      return 1 + longest_common_subsequence(begin1, last1, begin2, last2, op);
    } else {
      size_t lcs1, lcs2;

      #pragma omp task
      lcs1 = longest_common_subsequence(begin1, last1, begin2, end2, op);

      #pragma omp task
      lcs2 = longest_common_subsequence(begin1, end1, begin2, last2, op);

      #pragma omp taskkwait
      return std::max(lcs1, lcs2);
    }
  }
}


template <typename I1, typename I2, typename O = std::equal_to<typename std::iterator_traits<I1>::value_type>
  , typename = typename std::enable_if<std::is_same<typename std::iterator_traits<I1>::value_type, typename std::iterator_traits<I2>::value_type>::value>::type
>
size_t longest_common_subsequence(I1 begin1, I1 end1, I2 begin2, I2 end2, O&& op = {  }) {
  size_t result;

  #pragma omp parallel
  #pragma omp single
  result = detail::longest_common_subsequence(begin1, end1, begin2, end2, op);

  return result;
}


using clk = std::chrono::steady_clock;

/// Measures the exec time of a function and returns the result and time in seconds
/// https://en.cppreference.com/w/cpp/types/result_of See Notes section for why && is needed in result_of
/// @param func The function to call
/// @param args The arguments to pass into the function
/// @return A std::pair containing the time in seconds and the result
template <typename F, typename... A,
  typename = typename std::enable_if<!std::is_same<typename std::result_of<F&&(A&&...)>::type, void>::value>::type
>
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

  if (argc < 4) { std::cerr<<"usage: "<<argv[0]<<" <m> <n> <nbthreads>"<<std::endl;
    return -1;
  }

  int m = atoi(argv[1]);
  int n = atoi(argv[2]);
  omp_set_num_threads(atoi(argv[3]));


  // get string data
  char *X = new char[m];
  char *Y = new char[n];
  generateLCS(X, m, Y, n);


  int result = -1; // length of common subsequence
  float elapse;

  std::tie(elapse, result) = measure_func([=](){ return longest_common_subsequence(X, X + m, Y, Y + n); });


  // std::cout << result << std::endl;
  std::cerr << elapse << std::endl;
  checkLCS(X, m, Y, n, result);


  return 0;
}
