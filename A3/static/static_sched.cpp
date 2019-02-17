// Why must we use pthreads? why... whyyyyyy.... why not use the glory of c++11 std::threads????????

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <cmath>
#include <chrono>
using hrc = std::chrono::high_resolution_clock;
#include <utility>
#include <tuple>
#include <vector>
#include <memory>
#include <stdexcept>
#include <pthread.h>


using func_t = float (*)(float, int);
using pthread_func_t = void* (*) (void*);


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


/// This struct is to pass params into the threaded function
struct integrate_params {
  func_t functionid;
  int a;
  int b;
  int n;
  int sn;
  int en;
  float* answer;
  int intensity;
  pthread_mutex_t* lock;
};


/// The thread function for keeping a local variable and adding to shared variable before the thread dies
void* integrate_thread_partial(void* param_void) {
  auto p = static_cast<integrate_params*>(param_void);

  float ban = (p->b - p->a) / (float)p->n;
  float ans{  };

  for (int i = p->sn; i < p->en; ++i) {
    float x = p->a + ((float)i + 0.5) * ban;
    ans += p->functionid(x, p->intensity);
  }

  pthread_mutex_lock(p->lock);
  *p->answer += ans;
  pthread_mutex_unlock(p->lock);

  return nullptr;
}


/// The thread function for using a mutex to update one sigular result variable after each iteration
void* integrate_iteration_partial(void* param_void) {
  auto p = static_cast<integrate_params*>(param_void);

  float ban = (p->b - p->a) / (float)p->n;

  for (int i = p->sn; i < p->en; ++i) {
    float x = p->a + ((float)i + 0.5) * ban;
    float ans = p->functionid(x, p->intensity);

    pthread_mutex_lock(p->lock);
    *p->answer += ans;
    pthread_mutex_unlock(p->lock);
  }

  return nullptr;
}


/// This function is a wrapper to the partial integrator. Ittakes the function and aproximates the integral
/// using the parsed command line parameters. This sets up all the threads, and what each threads do.
/// @return The result of the integral and the time taken
/// @param functionid The function to integrate
/// @param a The upper bound of the integral
/// @param b The lower bound of the integral
/// @param n an integer which is the number of points to compute the approximation of the integral
/// @param intensity an integer which is the second parameter to give the the function to integrate
/// @param nbthreads The number of threads to use
/// @param partial The address of the sync function (either integrate_thread_partial or integrate_iteration_partial)
std::pair<float, float> integrate_wrapper(func_t functionid, int a, int b, int n, int intensity, int nbthreads, pthread_func_t partial) {
  auto timeStart = hrc::now();

  // set up data structs
  pthread_mutex_t lock;
  pthread_mutex_init(&lock, nullptr);
  std::vector<std::pair<pthread_t, integrate_params*>> threads{  };
  float result{  };

  // calculate work for each thread
  int depth = n / nbthreads;
  int start = 0;
  int end = depth;

  // start each thread and store the thread handle and its parameters (to prevent memory leaks)
  for (int i = 0; i < nbthreads - 1; ++i) {
    integrate_params* param_in = new integrate_params{ functionid, a, b, n, start, end, &result, intensity, &lock };
    pthread_t tmp{  };
    pthread_create(&tmp, nullptr, partial, param_in);
    threads.emplace_back(tmp, param_in);
    start = end;
    end += depth;
  }

  // start the last thread
  // As you can see here, if out partial function is thread, then fw will hold an array of floats and the array subscript
  // will return an seperate float for each thread. However id our partial function is iteration then fw will hold a single
  // float and the array subscripting will aways return the same float variable (but will be protected by a mutex)
  integrate_params* param_in = new integrate_params{ functionid, a, b, n, start, n, &result, intensity, &lock };
  pthread_t tmp{  };
  pthread_create(&tmp, nullptr, partial, param_in);
  threads.emplace_back(tmp, param_in);

  // wait for the threads to finish and cleanup memory
  for (auto& t : threads) {
    pthread_join(t.first, nullptr);
    delete t.second;
  }

  // calculate final result
  result *= (b - a) / (float)n;

  // calculate final time taken
  auto timeEnd = hrc::now();
  std::chrono::duration<double> elapse{ timeEnd - timeStart };
  float time = elapse.count(); // std::chrono::duration_cast<std::chrono::seconds>(elapse).count();

  return { result, time };
}


int main (int argc, char* argv[]) {

  if (argc < 8) {
    std::cerr<<"usage: "<<argv[0]<<" <functionid> <a> <b> <n> <intensity> <nbthreads> <sync>"<<std::endl;
    return -1;
  }

  int id = std::atoi(argv[1]);
  int a  = std::atoi(argv[2]);
  int b  = std::atoi(argv[3]);
  int n  = std::atoi(argv[4]);
  int i  = std::atoi(argv[5]);
  int nbthreads = std::atoi(argv[6]);

  // this section of code decides on which function should be threaded. If the parameter passed in is
  // iteration, then the function that will be threaded is integrate_iteration_partial and the FloatWrapper
  // will have one float element. Otherwise it will be integrate_thread_partial and the FloatWrapper will hold
  // an array (one for each thread).
  pthread_func_t partial = nullptr;
  switch (argv[7][0]) {
    case 'i': partial = integrate_iteration_partial; break;
    case 't': partial = integrate_thread_partial; break;

    default: {
      std::cout << "[E] What.... did.... you.... do....???" << std::endl;
      return -3;
    }
  }

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

#ifdef __cpp_structured_bindings
  auto [answer, timeTaken] = integrate_wrapper(func, a, b, n, i, nbthreads, partial);
#else
  float answer, timeTaken;
  std::tie(answer, timeTaken) = integrate_wrapper(func, a, b, n, i, nbthreads, partial);
#endif

  std::cout << answer << std::endl;
  std::cerr << timeTaken << std::endl;

  return 0;
}
