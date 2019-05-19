#include "mpi.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/stat.h"
#include <chrono>

#include <fstream>
#include <map>

int main(int argc, char **argv)
{
  if (argc < 3) {
    std::cerr<<"usage: ./wordfreq <filename> <count_threshold>"<<std::endl;
    return -1;
  }

  std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();

  std::string filename = std::string(argv[1]);
  uint32_t count_threshold = std::atoi(argv[2]);
  
  std::ifstream in (filename);

  std::map<std::string, int> count;
  
  while (in.good()) {
    std::string s;
    in >> s;
    if (in.good()) {
      count[s]++;
    }
  }

  for (auto& pa : count) {
    if (pa.second >= count_threshold)
      std::cout<<pa.second<<" "<<pa.first<<std::endl;
  }

  std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();

  std::chrono::duration<double> elapsed_seconds = end-start;

  std::cerr<<elapsed_seconds.count()<<std::endl;

  
  return 0;
  
}
