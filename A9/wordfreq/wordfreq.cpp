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
#include <fstream>
#include <memory>
#include <string>

using namespace MAPREDUCE_NS;

std::unique_ptr<char[]> nc_cstr(std::string str) {
  // ewwwwww....
  // https://stackoverflow.com/questions/1919626/
  std::unique_ptr<char[]> word{ new char[str.length() + 1] };
  std::copy(str.begin(), str.end(), word.get());
  word[str.length()] = '\0';
  return word;
}

void mymap(int id, char *filename, KeyValue *kv, void *params) {
  std::ifstream file{ filename };
  if (!file.is_open()) return;

  for (std::string str; file >> str; ) {
    kv->add(nc_cstr(str).get(), str.length() + 1, nullptr, 0);
  }
}

void myreduce(char *key, int keybytes, char *multivalue, int nvalues, int *valuebytes, KeyValue *kv, void *ptr) {
  kv->add(key, keybytes, (char *)&nvalues, sizeof(int));
}

int main(int argc, char **argv)
{
  if (argc < 3) {
    std::cerr<<"usage: ./wordfreq <filename> <count_threshold>"<<std::endl;
    return -1;
  }

  using clk = std::chrono::steady_clock;

  int rank, size;
  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  auto start = clk::now();

  MapReduce *map = new MapReduce{  };

  map->map(1, &argv[1], 0, 1, 0, &mymap, nullptr);
  map->collate(nullptr);

  map->reduce(&myreduce, nullptr);

  auto end = clk::now();

  std::cerr << std::chrono::duration<float>{end - start}.count() << std::endl;
  // using namespace string_literals;
  // std::string out_file = std::string{ "1output_" } + argv[1];
  // map->print(nc_cstr(out_file).get(), 0, -1, 1, 5, 1);

  MPI_Finalize();

  return 0;
}
