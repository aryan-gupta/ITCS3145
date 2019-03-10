#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif
  void generateMergeSortData (int* arr, size_t n);
  void checkMergeSortResult (int* arr, size_t n);
#ifdef __cplusplus
}
#endif

namespace detail {
// Merge Sort merge
template <typename I, typename O>
void merge_sort_merge(I begin, I mid, I end, O op) {
	std::vector<typename std::iterator_traits<I>::value_type> tmp{  };

  auto lbegin = begin;
	auto rbegin = mid;

	while (lbegin != mid and rbegin != end) {
		if (op(*lbegin, *rbegin))
			tmp.push_back(std::move(*lbegin++));
		else
			tmp.push_back(std::move(*rbegin++));
	}

	begin = std::move(tmp.begin(), tmp.end(), begin);

	begin = std::move(begin, mid, begin);
	std::move(rbegin, end, begin);
}
}


// Serial version of the merge sort
namespace serial {
template <typename I, typename O = std::less<typename I::value_type>,
          typename = std::enable_if_t<std::is_base_of<std::random_access_iterator_tag, typename std::iterator_traits<I>::iterator_category>::value>>
void merge_sort(I begin, I end, O op = {  }) {
  size_t size = std::distance(begin, end);
  size_t jump = 2;

  while (jump < size) {
    for (size_t i = 0; i < size; i += jump) {
      I e = begin + i + jump;
      if (e > end) e = end;
      I m = begin + i + (jump / 2);
      if (m > end) m = end;
      I b = begin + i;
      ::detail::merge_sort_merge(b, m, e, op);
    }
  }

}
}


int main (int argc, char* argv[]) {

  //forces openmp to create the threads beforehand
#pragma omp parallel
  {
    int fd = open (argv[0], O_RDONLY);
    if (fd != -1) {
      close (fd);
    }
    else {
      std::cerr<<"something is amiss"<<std::endl;
    }
  }

  if (argc < 3) { std::cerr<<"Usage: "<<argv[0]<<" <n> <nbthreads>"<<std::endl;
    return -1;
  }

  int n = atoi(argv[1]);

  // get arr data
  int * arr = new int [n];
  generateMergeSortData (arr, n);

  //insert sorting code here.



  checkMergeSortResult (arr, n);

  delete[] arr;

  return 0;
}
