// Why must we use pthreads? why... whyyyyyy.... why not use the glory of c++11 std::threads????????

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
#include <stdexcept>

#include <pthread.h>

/// This code got very complicated really fast. I will try to explain as best as I can. The integrate_wrapper
/// function sets up the work that each thread will do and starts each thread.

using hrc = std::chrono::high_resolution_clock;

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


class DynamicSchedular {
public:
  using timeline_t = std::vector<std::chrono::time_point<hrc>>;

private:
  pthread_mutex_t mMux;
  const int mEnd;
  const int mGran;
  int mCurrent;
  // @todo add a done mutex or lock-free apprach.
  // No atomics in pthread: https://stackoverflow.com/questions/1130018 :((((((
  bool mDone;

  // The idealogy behind this is that. Each thread will update its own timeline when it calls get.
  // At the end, before the thread dies, it will move its data into the master table. Because
  // moveing a vector is as simple as copying a few pointers, I will be including it in the time
  // as compared to stopping the time when the threads finish then moving the data over.
  // mTable will not needed to be protected by a mutex because each element will only be accessed
  // by one thread
  // thread_local must be declared static. makes sense, I guess
  static thread_local timeline_t mTimeLine;
  timeline_t* mTable;
  std::chrono::time_point<hrc> mStartTime;

public:

  DynamicSchedular(int end, int gran, unsigned nbthreads) : mEnd{ end }, mGran{ gran }, mCurrent{ 0 }, mDone{ false } {
    pthread_mutex_init(&mMux, nullptr);
    mTable = new timeline_t[nbthreads];
  }

  std::pair<int, int> get(int id) {
    mTimeLine.push_back(hrc::now());

    int start, end;
    pthread_mutex_lock(&mMux);
    if (mCurrent >= mEnd or mDone) {
      start = end = 0;
      mDone = true;

      // Ownership gets transfered here
      mTable[id] = std::move(mTimeLine);
    } else {
      /// @todo fix the edge cases for this
      start = mCurrent;
      end = mCurrent = mCurrent + mGran;
    }
    pthread_mutex_unlock(&mMux);
    return { start, end };
  }

  bool done() {
    // for now
    pthread_mutex_lock(&mMux);
    bool done = mDone;
    pthread_mutex_unlock(&mMux);
    return done;
  }

  // @warning This function is not thread-safe
  void set_start_time(std::chrono::time_point<hrc>& tm) {
    mStartTime = tm;
  }

  /// @warning This function is not thread-safe
  std::pair<timeline_t*, std::chrono::time_point<hrc>> get_table() {
    return { mTable, mStartTime };
  }
};

thread_local DynamicSchedular::timeline_t DynamicSchedular::mTimeLine = {  };

/// This struct is to pass params into the threaded function
struct integrate_params {
  func_t functionid;
  int a;
  int b;
  int n;
  float* answer;
  int intensity;
  DynamicSchedular* sched;
  int id;
};


pthread_mutex_t lock;

/// The thread function for keeping a local variable and at the end adding up all the partial values
void* integrate_thread_partial(void* param_void) {
  auto p = static_cast<integrate_params*>(param_void);

  float ban = (p->b - p->a) / (float)p->n;
  float ans{  };

  while (!p->sched->done()) {
    int start, end;
    std::tie(start, end) = p->sched->get(p->id);
    for (int i = start; i < end; ++i) {
      float x = p->a + ((float)i + 0.5) * ban;
      ans += p->functionid(x, p->intensity);
    }
  }

  pthread_mutex_lock(&lock);
  *p->answer += ans;
  pthread_mutex_unlock(&lock);

  return nullptr;
}


/// The thread function for keeping a local variable and at the end adding up all the partial values
void* integrate_chunk_partial(void* param_void) {
  auto p = static_cast<integrate_params*>(param_void);

  float ban = (p->b - p->a) / (float)p->n;

  while (!p->sched->done()) {
    float ans{  };
    int start, end;
    std::tie(start, end) = p->sched->get(p->id);
    for (int i = start; i < end; ++i) {
      float x = p->a + ((float)i + 0.5) * ban;
      ans += p->functionid(x, p->intensity);
    }
    pthread_mutex_lock(&lock);
    *p->answer += ans;
    pthread_mutex_unlock(&lock);
  }

  return nullptr;
}


void* integrate_iteration_partial(void* param_void) {
  auto p = static_cast<integrate_params*>(param_void);

  float ban = (p->b - p->a) / (float)p->n;

  // get chunk from Schedular
  while (!p->sched->done()) {
    int start, end;
    std::tie(start, end) = p->sched->get(p->id);
    for (int i = start; i < end; ++i) {
      float x = p->a + ((float)i + 0.5) * ban;
      float ans = p->functionid(x, p->intensity);

      pthread_mutex_lock(&lock);
      *p->answer += ans;
      pthread_mutex_unlock(&lock);
    }
  }

  return nullptr;
}


/// Wrapper for calling the partial integrator. This sets up all the threads, and what each threads do.
/// Yes, I understand that I am passing 9 parameters to this function.
std::pair<float, float> integrate_wrapper(func_t functionid, int a, int b, int n, int intensity, int nbthreads, pthread_func_t partial, DynamicSchedular* sched) {
  auto timeStart = hrc::now();
  sched->set_start_time(timeStart);

  // if we are doing interation then we need to set up the mutex
  pthread_mutex_init(&lock, nullptr);
  std::vector<std::pair<pthread_t, integrate_params*>> threads{  };
  float result{  };

  // calculate work for each thread
  int depth = n / nbthreads;
  int start = 0;
  int end = depth;

  // start each thread and store the thread handle and its parameters (to prevent memory leaks)
  for (int i = 0; i < nbthreads - 1; ++i) {
    integrate_params* param_in = new integrate_params{ functionid, a, b, n, &result, intensity, sched, i };
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
  integrate_params* param_in = new integrate_params{ functionid, a, b, n, &result, intensity, sched, nbthreads - 1 };
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
  float time = elapse.count();

  return { result, time };
}

void process_and_print(typename DynamicSchedular::timeline_t* data, unsigned nbthreads, std::chrono::time_point<hrc>& start) {
  for (int i = 0; i < nbthreads; ++i) {
    auto vec = data[i];
    for (auto& tp : vec) {
        std::chrono::duration<double> elapse{ tp - start };
        float time = elapse.count();
        std::cout << time << "    ";
    }
    std::cout << std::endl;
  }
}


int main (int argc, char* argv[]) {

  if (argc < 9) {
    std::cerr<<"usage: "<<argv[0]<<" <functionid> <a> <b> <n> <intensity> <nbthreads> <sync> <granularity>"<<std::endl;
    return -1;
  }

  int id = std::atoi(argv[1]);
  int a  = std::atoi(argv[2]);
  int b  = std::atoi(argv[3]);
  int n  = std::atoi(argv[4]);
  int i  = std::atoi(argv[5]);
  unsigned nbthreads = std::atoi(argv[6]);
  int granularity = std::atoi(argv[8]);

  // maybe use string_view to make this more robust? rather than comparing the first char?
  // on second thought, string_view is c++17, rather not use it, compiler may not support

  // this section of code decides on which function should be threaded. If the parameter passed in is
  // iteration, then the function that will be threaded is integrate_iteration_partial and the FloatWrapper
  // will have one float element. Otherwise it will be integrate_thread_partial and the FloatWrapper will hold
  // an array (one for each thread).
  pthread_func_t partial = nullptr;
  switch (argv[7][0]) {
    case 'i': partial = integrate_iteration_partial; break;
    case 't': partial = integrate_thread_partial; break;
    case 'c': partial = integrate_chunk_partial; break;

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

  DynamicSchedular ds{ n, granularity, nbthreads };

#ifdef __cpp_structured_bindings
  auto [answer, timeTaken] = integrate_wrapper(func, a, b, n, i, nbthreads, partial, &ds);
  auto [dsData, startTime] = ds.get_table();
#else
  float answer, timeTaken;
  std::tie(answer, timeTaken) = integrate_wrapper(func, a, b, n, i, nbthreads, partial, &ds);

  typename DynamicSchedular::timeline_t* dsData;
  std::chrono::time_point<hrc> startTime;
  std::tie(dsData, startTime) = ds.get_table();
#endif

  process_and_print(dsData, nbthreads, startTime);

  std::cout << answer << std::endl;
  std::cerr << timeTaken << std::endl;

  return 0;
}
