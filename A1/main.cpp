#include <iostream>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <utility>
#include <tuple>
using hrc = std::chrono::high_resolution_clock;

// Im pretty sure that c doesnt have using keyword so Ill
// declare this out here
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
float integrate(func_t func, float a, float b, float n, int t) {
  float ban = (b - a) / n;
  float ans{  };

  for (float i = 0; i < n - 1; ++i) {
    float x = a + (i + 0.5) * ban;
    ans += func(x, t);
  }

  return ans * ban;
}

std::pair<float, float> integrate_wrapper(func_t func, float a, float b, float n, int t) {
  auto start = hrc::now();
  float ans = integrate(func, a, b, n, t);
  auto end = hrc::now();

  float seconds = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();

  return { ans, seconds };
}

int main (int argc, char* argv[]) {

  if (argc < 6) {
    std::cerr<<"usage: "<<argv[0]<<" <functionid> <a> <b> <n> <intensity>"<<std::endl;
    return -1;
  }

  int id = std::atoi(argv[1]);
  int a  = std::atoi(argv[2]);
  int b  = std::atoi(argv[3]);
  int n  = std::atoi(argv[4]);
  int i  = std::atoi(argv[5]);

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

// make file does not have c++17, I cry every night
#if __has_cpp_attribute(__cpp_structured_bindings)
  auto [answer, timeTaken] = integrate_wrapper(func, a, b, n, i);
#else
  float answer, timeTaken;
  std::tie(answer, timeTaken) = integrate_wrapper(func, a, b, n, i);
#endif

  std::cout << answer << std::endl;
  std::cerr << timeTaken << std::endl;

  return 0;
}
