#include <omp.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

#include <vector>
#include <iterator>
#include <iostream>
#include <chrono>
using clk = std::chrono::steady_clock;

#ifdef __cplusplus
extern "C" {
#endif
  void generateMergeSortData (int* arr, size_t n);
  void checkMergeSortResult (int* arr, size_t n);
#ifdef __cplusplus
}
#endif

/// Recurively prints out a series of printable objects, cause I cant be bothered to write std::cout << X << std::endl
template <typename T>
void print(T &&t) {
  std::cout << t;
}

template <typename T, typename... A>
void print(T &&t, A &&...args) {
  std::cout << t;
  print( std::forward<A>(args)... );
}

template <typename... A>
void println(A &&...args) {
  print(std::forward<A>(args)...  );
  std::cout << std::endl;
}

/// Prints out a range between 2 iterators
template <typename I>
void print(I begin, I end) {
  using ValueType = typename std::iterator_traits<I>::value_type;
  std::copy(begin, end, std::ostream_iterator<ValueType>{ std::cout, " "});
  std::cout << std::endl;
}


/// Measures the exec time of a function and returns the result and time in seconds
/// @param func The function to call
/// @param args The arguments to pass into the function
/// @return A std::pair containing the time in seconds and the result
template <typename F, typename... A>
auto measure_func(F func, A &&...args) -> std::pair<float, typename std::result_of<F>::type> {
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
template<typename F, typename... A>
float measure_func(F func, A &&...args) {
  auto start = clk::now();
  func( std::forward<A>(args)... );
  auto end = clk::now();
  std::chrono::duration<float> elapse = end - start;
  return elapse.count();
}


namespace detail {
// Merge Sort merge
template <typename I, typename O>
void merge_sort_merge(I begin, I mid, I end, O op) {
  std::vector<typename std::iterator_traits<I>::value_type> tmp{  };

  auto lbegin = begin;
  auto rbegin = mid;

  while (lbegin != mid and rbegin != end) {
    if (op(*lbegin, *rbegin))
      tmp.push_back(std::move(*lbegin++));
    else
      tmp.push_back(std::move(*rbegin++));
  }

  std::move(lbegin, mid, std::back_inserter(tmp));
  std::move(rbegin, end, std::back_inserter(tmp));
  std::move(tmp.begin(), tmp.end(), begin);
}
}


// Serial version of the merge sort
namespace serial {
template <typename I, typename O = std::less<typename std::iterator_traits<I>::value_type>,
          typename = typename std::enable_if<std::is_base_of<std::random_access_iterator_tag, typename std::iterator_traits<I>::iterator_category>::value>::type>
void merge_sort(I begin, I end, O op = {  }) {
  size_t size = std::distance(begin, end);
  size_t jump = 1;

  while (jump < size) {
    jump *= 2;
    for (size_t i = 0; i < size; i += jump) {
      I e = begin + i + jump;
      if (e > end) e = end;
      I m = begin + i + (jump / 2);
      if (m > end) m = end;
      I b = begin + i;
      ::detail::merge_sort_merge(b, m, e, op);
    }
  }
}
}


namespace parallel {

template <typename I, typename T = typename std::iterator_traits<I>::value_type, typename O = std::less<T>>
I binary_search(I begin, I end, const T& value, O op = {  }) {
  size_t size = std::distance(begin, end);
  if (size <= 1) return begin;

  I mid = begin + (size / 2);
  if (op(value, *mid))
    return binary_search(begin, mid);
  else
    return binary_search(mid, end);
}

template <typename I, typename O = std::less<typename std::iterator_traits<I>::value_type>,
          typename = typename std::enable_if<std::is_base_of<std::random_access_iterator_tag, typename std::iterator_traits<I>::iterator_category>::value>::type>
void merge_sort(I begin, I end, O op = {  }) {
  size_t size = std::distance(begin, end);
  size_t jump = 1;

  while (jump < size) {
    jump *= 2;
    #pragma omp parallel for
    for (size_t i = 0; i < size; i += jump) {
      I e = begin + i + jump;
      if (e > end) e = end;
      I m = begin + i + (jump / 2);
      if (m > end) m = end;
      I b = begin + i;
      ::detail::merge_sort_merge(b, m, e, op);
    }
  }
}
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

  if (argc < 3) { std::cerr<<"Usage: "<<argv[0]<<" <n> <nbthreads>"<<std::endl;
    return -1;
  }

  int n = atoi(argv[1]);
  int nbthreads = atoi(argv[2]);
  omp_set_num_threads(nbthreads);

  // get arr data
  int * arr = new int [n];
  generateMergeSortData (arr, n);
 float elapse = measure_func( parallel::merge_sort<int*>, arr, arr + n, std::less<int>{} );
  std::cerr << elapse << std::endl;

  checkMergeSortResult (arr, n);

  delete[] arr;

  return 0;
}
