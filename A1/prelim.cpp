#include <unistd.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <limits.h>

int main () {

  char str[HOST_NAME_MAX + 1];
  int ret = gethostname(str, HOST_NAME_MAX + 1);

  if (ret != 0) {
    std::cout << "Somthing went wrong" << std::endl;
    return -1;
  }

  // std::ofstream file{ "preliminary_answer" };
  // file << str.get() << std::endl;
  // file.close();

  std::cout << str << std::endl;

  return 0;
}
