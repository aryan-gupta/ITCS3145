#include <iostream>
#include <cmath>
#include <cstdlib>
#include <chrono>

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

int main (int argc, char* argv[]) {

  if (argc < 6) {
    std::cerr<<"usage: "<<argv[0]<<" <functionid> <a> <b> <n> <intensity>"<<std::endl;
    return -1;
  }

  int   id = std::atoi(argv[1]);
  int   a  = std::atoi(argv[2]);
  int   b  = std::atoi(argv[3]);
  int   n  = std::atoi(argv[4]);
  float i  = std::atof(argv[5]);

  float answer{  };

  switch (id) {
    case 1: answer = integrate(f1, a, b, n, i); break;
    case 2: answer = integrate(f2, a, b, n, i); break;
    case 3: answer = integrate(f3, a, b, n, i); break;
    case 4: answer = integrate(f4, a, b, n, i); break;
    default: {
      std::cout << "[E] Ya you screwed up... have fun" << std::endl;
      return -2;
    }
  }

  std::cout << answer << std::endl;

  return 0;
}
