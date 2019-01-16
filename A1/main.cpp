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
/// @return The result of the integral
/// @param functionid The function to integrate
/// @param a The upper bound of the integral
/// @param b The lower bound of the integral
/// @param n an integer which is the number of points to compute the approximation of the integral
/// @param intensity an integer which is the second parameter to give the the function to integrate
float integrate(func_t functionid, float a, float b, float n, int intensity) {
  float ban = (b - a) / n;
  float ans{  };

  for (int i = 0; i < n; ++i) {
    float x = a + ((double)i + 0.5) * ban;
    ans += functionid(x, intensity);
  }

  return ans * ban;
}


/// This is a simple wrapper that times the integrate function. Returns the time it took
/// and the answer of the integral. The parameters are passed to the integrate function
/// Yes I am extra, there is no need for this function to be a vardic template, but why not?
/// @return A pair of floats. The first is the answer and the second is the time it took
/// @parameter pack A parameter pack that is forwarded to the integrate function by perfect fwding
template <typename... PP>
std::pair<float, float> integrate_wrapper(PP&&... pack) {
  auto start = hrc::now();
  float ans = integrate(std::forward<PP>(pack)...);
  auto end = hrc::now();

  std::chrono::duration<double> elapse{ end - start };
  float seconds = elapse.count(); // std::chrono::duration_cast<std::chrono::seconds>(elapse).count();

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
// Ok so turns out I also needs to check if __has_cpp_attibute exists too. I'm going to do this for now
// but Im probs displeasing the C++ gods
#ifdef __cpp_structured_bindings
  auto [answer, timeTaken] = integrate_wrapper(func, a, b, n, i);
#else
  float answer, timeTaken;
  std::tie(answer, timeTaken) = integrate_wrapper(func, a, b, n, i);
#endif

  std::cout << answer << std::endl;
  std::cerr << timeTaken << std::endl;

  return 0;
}
