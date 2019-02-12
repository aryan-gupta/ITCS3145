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

// #include <mutex>

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

/// This class abstracts away an array vs single element of data. Depending on how the object
/// is constructed, It will keep 1 element, or an array of element.
/// This is to reduce code redundancy. The two types of static threading models have majoritly the
/// same amount of code, the different parts are which function below it calls to run as a thread
/// One model keeps a float value local to the thread and the other uses a mutex to update one sigular
/// value. This class abstracts this away. If the FloatWrapper class is init as one float value
/// and use a mutex, then only one float value is created and any access to the "array" just calles
/// the one value. I am aware of how hacky this is because when you use the array subscripting you assume
/// that FloatWrapper[x] and FloatWrapper[y] will return two differnet locatons. But I will have to
/// change up the schematics a bit to fix this.
class FloatWrapper {
  float* mData;
  size_t mSize;

public:
  FloatWrapper(size_t size) : mData{  }, mSize{ size } {
    if (mSize == 0) throw std::invalid_argument{ "[E] Why am I doin this?" };
    else if (mSize == 1) mData = new float{  };
    else mData = new float[mSize];
  }

  // honestly this is because Im too lazy to write these funcs
  FloatWrapper(FloatWrapper& ) = delete; // NO COPY CTOR
  FloatWrapper(FloatWrapper&& ) = delete; // NO MOVE CTOR
  FloatWrapper& operator= (FloatWrapper& ) = delete; // NO COPY ASSIGN
  FloatWrapper& operator= (FloatWrapper&& ) = delete; // NO MOVE ASSIGN

  ~FloatWrapper() {
    if (mSize == 1) delete mData;
    else delete[] mData;
  }

  // @todo const versions of these functions
  float& operator[] (size_t idx) {
    if (mSize == 1) return *mData;
    return mData[idx];
  }

  float get() {
    if (mSize == 1) return *mData;

    float result{  };
    for (int i = 0; i < mSize; ++i)
    result += mData[i];

    return result;
  }
};

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

/// The thread function for keeping a local variable and at the end adding up all the partial values
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

/// The thread function for using a mutex to update one sigular result variable. the mutex prevents
/// a race condition.
pthread_mutex_t lock;
void* integrate_iteration_partial(void* param_void) {
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


/// Wrapper for calling the partial integrator. This sets up all the threads, and what each threads do.
std::pair<float, float> integrate_wrapper(func_t functionid, int a, int b, float n, int intensity, int nbthreads, FloatWrapper& fw, pthread_func_t partial) {
  auto timeStart = hrc::now();

  pthread_mutex_init(&lock, nullptr); // What is this error checking you speak of?
  std::vector<std::pair<pthread_t, integrate_params*>> threads{  };

  int depth = n / nbthreads;
  int start = 0;
  int end = depth;

  for (int i = 0; i < nbthreads - 1; ++i) {
    integrate_params* param_in = new integrate_params{ functionid, a, b, n, start, end, &fw[i], intensity };
    pthread_t tmp{  };
    pthread_create(&tmp, nullptr, partial, param_in);
    threads.emplace_back(tmp, param_in);
    start = end;
    end += depth;
  }

  integrate_params* param_in = new integrate_params{ functionid, a, b, n, start, n, &fw[nbthreads - 1], intensity };
  pthread_t tmp{  };
  pthread_create(&tmp, nullptr, partial, param_in);
  threads.emplace_back(tmp, param_in);

  for (auto& t : threads) {
    pthread_join(t.first, nullptr);
    delete t.second;
  }

  float result = fw.get() * (b - a) / n;

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

  // maybe use string_view to make this more robust? rather than comparing the first char?
  // on second thought, string_view is c++17, rather not use it, compiler may not support
  pthread_func_t partial = nullptr;
  std::unique_ptr<FloatWrapper> fw{  };
  switch (argv[7][0]) {
    case 'i': {
      partial = integrate_iteration_partial;
      fw.reset(new FloatWrapper{ 1 } );
    } break;

    case 't': {
      partial = integrate_thread_partial;
      fw.reset(new FloatWrapper{ nbthreads } );
    } break;

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
  auto [answer, timeTaken] = integrate_wrapper(func, a, b, n, i, nbthreads, *fw.get(), partial);
#else
  float answer, timeTaken;
  std::tie(answer, timeTaken) = integrate_wrapper(func, a, b, n, i, nbthreads, *fw.get(), partial);
#endif

  std::cout << answer << std::endl;
  std::cerr << timeTaken << std::endl;

  return 0;
}
