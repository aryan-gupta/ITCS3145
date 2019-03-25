#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

#include <omp.h>

#include <iterator>
#include <chrono>
#include <type_traits>

#ifdef __cplusplus
extern "C" {
#endif

  void generateMergeSortData (int* arr, size_t n);
  void checkMergeSortResult (const int* arr, size_t n);

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


// bool was unused because of https://stackoverflow.com/questions/21589411
namespace detail {
template <typename I, typename O>
void bubblesort(I begin, I end, O op) {
  char swapped = 0;
  size_t iteration = 0;

  do {
    swapped = 0;

    I ptr = (iteration++ % 2 == 0)? begin : std::next(begin);
    I last = std::next(end, -1);

    for (; ptr != last and ptr != end; std::advance(ptr, 2)) {
      #pragma omp task firstprivate(ptr) shared(swapped)
      {
        I next = std::next(ptr);
        if (!op(*ptr, *next)) {
          std::iter_swap(ptr, next);
          #pragma omp atomic
          swapped |= 1;
        }
      }
    }
    #pragma omp taskwait
  } while (swapped != 0);
}
} // end namespace detail

template <typename I, typename O = std::less<typename std::iterator_traits<I>::value_type>,
          typename = typename std::enable_if<std::is_base_of<std::random_access_iterator_tag, typename std::iterator_traits<I>::iterator_category>::value>::type>
void bubblesort(I begin, I end, O op = {  }) {
  #pragma omp parallel
  #pragma omp single
  {
    detail::bubblesort(begin, end, op);
  }
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

  if (argc < 3) { std::cerr<<"usage: "<<argv[0]<<" <n> <nbthreads>"<<std::endl;
    return -1;
  }

  int n = atoi(argv[1]);
  omp_set_num_threads(atoi(argv[2]));

  // get arr data
  int * arr = new int [n];
  generateMergeSortData (arr, n);
  // print(arr, arr + n);

  auto elapse = measure_func([=](){ bubblesort(arr, arr + n); });

  // print(arr, arr + n);


  std::cerr << elapse << std::endl;
  checkMergeSortResult (arr, n);

  delete[] arr;

  return 0;
}
