#include <unistd.h>
#include <iostream>
#include <fstream>
#include <memory>

int main () {
  constexpr size_t CHAR_LEN = 1024;

  std::unique_ptr<char> str{ new char[CHAR_LEN] };
  int ret = gethostname(str.get(), CHAR_LEN);

  if (ret != 0) {
    std::cout << "Somthing went wrong" << std::endl;
    return -1;
  }

  std::ofstream file{ "preliminary_answer" };
  file << str.get() << std::endl;
  file.close();

  return 0;
}
