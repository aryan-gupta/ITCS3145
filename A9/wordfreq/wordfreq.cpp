#include "mpi.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/stat.h"
#include "mapreduce.h"
#include "keyvalue.h"
#include <chrono>
#include <sstream>
#include <iostream>

using namespace MAPREDUCE_NS;

int main(int argc, char **argv)
{
  if (argc < 3) {
    std::cerr<<"usage: ./wordfreq <filename> <count_threshold>"<<std::endl;
    return -1;
  }


}
