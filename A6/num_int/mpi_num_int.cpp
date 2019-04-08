#include <iostream>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <mpi.h>

#include <utility>
#include <chrono>
using clk = std::chrono::steady_clock;

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


/// This function takes the function and aproximates the integral using
/// the parsed command line parameters. Im also making all the parameters
/// float cause I dont want to cast them everytime.
/// @return The result of the integral
/// @param functionid The function to integrate
/// @param a The upper bound of the integral
/// @param b The lower bound of the integral
/// @param n an integer which is the number of points to compute the approximation of the integral
/// @param intensity an integer which is the second parameter to give the the function to integrate
float integrate(func_t functionid, int a, int b, int start, int end, int n, int intensity) {
  float ban = (b - a) / (float)n;
  float ans{  };

  for (int i = start; i < end; ++i) {
    float x = a + ((float)i + 0.5) * ban;
    ans += functionid(x, intensity);
  }

  return ans;
}


/// @return whether we are rank 0 and need to output results, and the global anserr
std::pair<bool, float> mpi_integrate(func_t functionid, float a, float b, float n, int intensity) {
  int rank = 0, size = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  int depth = n / size; // number of integrations each node will take
  int start = rank * depth;
  int end = (rank != size)? (rank + 1) * depth : n;


  float local_ans = integrate(functionid, a, b, start, end, n, intensity);

  float global_ans = 0;
	MPI_Reduce(&local_ans, &global_ans, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
  global_ans *= (b - a) / (float)n;

  return std::make_pair(rank == 0, global_ans);
}


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

  if (argc < 6) {
    std::cerr<<"usage: "<<argv[0]<<" <functionid> <a> <b> <n> <intensity>"<<std::endl;
    return -1;
  }


  int rank = 0, size = 0;

  MPI_Init(&argc, &argv);

  int id = std::atoi(argv[1]);
  int a  = std::atoi(argv[2]);
  int b  = std::atoi(argv[3]);
  int n  = std::atoi(argv[4]);
  int i  = std::atoi(argv[5]);

  // depending on the function id. Set the function pointer to the pointer to the function
  func_t func = nullptr;
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

  float elapse;
  std::pair<bool, float> answer;
  std::tie(elapse, answer) = measure_func(mpi_integrate, func, a, b, n, i);

  if (answer.first) {
    std::cout << answer.second << std::endl;
    std::cerr << elapse << std::endl;
  }


  MPI_Finalize();
  return 0;
}
