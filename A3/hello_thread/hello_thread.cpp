#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <utility>

#include <pthread.h>

// #define USE_MUTEX

#ifdef USE_MUTEX
  #include <mutex>
  static std::mutex stdout_mux{  };
#endif

//
void* pthread_work(void* id_var) {
  int id = *static_cast<int*>(id_var);
#ifdef USE_MUTEX
  std::unique_lock<std::mutex> lk{ stdout_mux };
#endif
  std::cout << "I am thread " << id << " in nbthreads" << std::endl;
  return nullptr;
}

int main (int argc, char* argv[]) {

  if (argc < 2) {
    std::cerr<<"usage: "<<argv[0]<<" <nbthreads>"<<std::endl;
    return -1;
  }

  int nbthreads = std::atoi(argv[1]); // yes I know, no error checking
  if (nbthreads == 0)
    std::cout << "[E] Whoa there buckaroo, you cant have " << argv[1] << " threads" << std::endl;

  // std::cout << "Starting " << nbthreads << " threads..." << std::endl;

  // I could make the int* into a unique_ptr<int> but......
  std::vector<std::pair<int*, pthread_t>> threads{  };
  for (int i = 0; i < nbthreads; ++i) {
    pthread_t tmp{  };
    int* id = new int{ i };
    pthread_create(&tmp, nullptr, pthread_work, static_cast<void*>(id));
    threads.emplace_back(id, tmp);
  }

  for (auto& t : threads) {
    pthread_join(t.second, nullptr);
    delete t.first;
  }

  return 0;
}
