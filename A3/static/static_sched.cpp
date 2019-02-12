#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <cmath>
#include <chrono>
#include <utility>
#include <tuple>
#include <vector>
#include <memory>

#include <pthread.h>

using hrc = std::chrono::high_resolution_clock;

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

struct integrate_params {
  func_t functionid;
  float a;
  float b;
  float n;
  float sn;
  float en;
  float* answer;
  int intensity;
};

void* integrate_thread_partial(void* param_void) {
  auto p = static_cast<integrate_params*>(param_void);

  float ban = (p->b - p->a) / p->n;
  float ans{  };

  for (int i = p->sn; i < p->en; ++i) {
    float x = p->a + ((float)i + 0.5) * ban;
    ans += p->functionid(x, p->intensity);
  }

  *p->answer = ans;

  return nullptr;
}


std::pair<float, float> integrate_thread_wrapper(func_t functionid, int a, int b, float n, int intensity, int nbthreads) {
  auto timeStart = hrc::now();

  std::vector<std::pair<pthread_t, integrate_params*>> threads{  };
  std::unique_ptr<float[]> answers{ new float[nbthreads] };

  int depth = n / nbthreads;
  int start = 0;
  int end = depth;

  for (int i = 0; i < nbthreads - 1; ++i) {
    integrate_params* param_in = new integrate_params{ functionid, a, b, n, start, end, &answers[i], intensity };
    pthread_t tmp{  };
    pthread_create(&tmp, nullptr, integrate_thread_partial, param_in);
    threads.emplace_back(tmp, param_in);
    start = end;
    end += depth;
  }

  integrate_params* param_in = new integrate_params{ functionid, a, b, n, start, n, &answers[nbthreads - 1], intensity };
  pthread_t tmp{  };
  pthread_create(&tmp, nullptr, integrate_thread_partial, param_in);
  threads.emplace_back(tmp, param_in);

  for (auto& t : threads) {
    pthread_join(t.first, nullptr);
    delete t.second;
  }

  float result{  };
  for (int i = 0; i < nbthreads; ++i)
    result += answers[i];

  result *= (b - a) / n;

  auto timeEnd = hrc::now();
  std::chrono::duration<double, std::milli> elapse{ timeEnd - timeStart };
  float time = elapse.count(); // std::chrono::duration_cast<std::chrono::seconds>(elapse).count();

  return { result, time };
}


pthread_mutex_t lock;
void* integrate_partial_iteration(void* param_void) {
  auto p = static_cast<integrate_params*>(param_void);

  float ban = (p->b - p->a) / p->n;

  for (int i = p->sn; i < p->en; ++i) {
    float x = p->a + ((float)i + 0.5) * ban;
    float ans = p->functionid(x, p->intensity);

    pthread_mutex_lock(&lock);
    *p->answer += ans;
    pthread_mutex_unlock(&lock);
  }

  return nullptr;
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

  // maybe use string_view to make this more robust? rather than comparing the first char?
  // on second thought, string_view is c++17, rather not use it, compiler may not support
  bool threadLocal = false;
  switch (argv[7][0]) {
    case 'i': threadLocal = false; break;
    case 't': threadLocal = true;  break;
    default: {
      std::cout << "[E] What.... did.... you.... do....???" << std::endl;
      return -3;
    }
  }

  func_t func = nullptr;

  // depending on the function id. Set the function pointer to the pointer to the function
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
  auto [answer, timeTaken] = integrate_thread_wrapper(func, a, b, n, i, nbthreads);
#else
  float answer, timeTaken;
  std::tie(answer, timeTaken) = integrate_thread_wrapper(func, a, b, n, i, nbthreads);
#endif

  std::cout << answer << std::endl;
  std::cerr << timeTaken << std::endl;

  return 0;
}
