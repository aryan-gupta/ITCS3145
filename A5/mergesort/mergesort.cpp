#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

#include <cassert>
#include <chrono>
#include <vector>
#include <iterator>

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

template <typename I, typename O>
void merge_sort_merge_recurse(I begin, I end, O op) {
  auto dist = std::distance(begin, end);
  if (dist <= 1) return;

	auto mid = std::next(begin, dist / 2);

  #pragma omp taskgroup
  {

    #pragma omp task
    merge_sort_merge_recurse(begin, mid, op);

    #pragma omp task
    merge_sort_merge_recurse(mid, end, op);

  }

  #pragma omp task
  merge_sort_merge(begin, mid, end, op);

}

} // end namespace detail


template <typename I, typename O = std::less<typename std::iterator_traits<I>::value_type>,
          typename = typename std::enable_if<std::is_base_of<std::random_access_iterator_tag, typename std::iterator_traits<I>::iterator_category>::value>::type>
void merge_sort(I begin, I end, O op = {  }) {
  #pragma omp parallel
  #pragma omp single
  {
    detail::merge_sort_merge_recurse(begin, end, op);
  }
}



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

  if (argc < 3) { std::cerr<<"usage: "<<argv[0]<<" <n> <nbthreads>"<<std::endl;
    return -1;
  }

  int n = atoi(argv[1]);

  // get arr data
  int * arr = new int [n];
  generateMergeSortData (arr, n);

  float elapse = measure_func(merge_sort<int*, std::less<int>, void>, arr, arr + n, std::less<int>{  });

  std::cerr << elapse << std::endl;
  checkMergeSortResult (arr, n);

  delete[] arr;

  return 0;
}
