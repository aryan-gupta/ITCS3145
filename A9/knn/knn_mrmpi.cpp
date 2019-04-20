#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <tuple>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <map>
#include <mapreduce.h>
#include <keyvalue.h>
#include <mpi.h>
#include <cstring>
#include <chrono>
#include <numeric>

typedef int classes;
typedef float feature;


int main (int argc, char* argv[]) {
  if (argc < 4) {
    std::cerr<<"usage: "<<argv[0]<<" <databasefile> <queryfile> <k>"<<std::endl;
    return -1;
  }

  return 0;
}
