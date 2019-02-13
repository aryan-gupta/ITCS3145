

#include <vector>
#include <random>
#include <algorithm>
#include <iostream>

// This class serves the purpose of testing that there are no useless
// copies of data. The copy functions are deleted meaning there when an
// object is constructed, it cannot be copied, only moved
class NoCopy {
	int data;

public:
	NoCopy() = default;
	NoCopy(int d) : data{ d } {  };

	NoCopy(NoCopy&) = delete;
	NoCopy& operator= (NoCopy&) = delete;

	/// WE CAN MOVE THIS
	NoCopy(NoCopy&&) = default;
	NoCopy& operator= (NoCopy&&) = default;

	// here we goooooo........
	// well thats great no default compare operators until c++20
	bool operator<  (const NoCopy& o) const { return data <  o.data; }
	bool operator<= (const NoCopy& o) const { return data <= o.data; }
	bool operator>  (const NoCopy& o) const { return data >  o.data; }
	bool operator>= (const NoCopy& o) const { return data >= o.data; }
	bool operator== (const NoCopy& o) const { return data == o.data; }
	bool operator!= (const NoCopy& o) const { return data != o.data; }
};


// No need for SFINE here because it will only be called by merge_sort, meaning the types will
// be corect
template <typename I, typename O>
std::vector<typename I::value_type> merge_sort_merge(I begin, I mid, I end, O op) {
	std::vector<typename std::iterator_traits<I>::value_type> tmp{  };

	auto rbegin = mid;

	while (begin != mid and rbegin != end) {
		if (op(*begin, *rbegin))
			tmp.push_back(std::move(*begin++));
		else
			tmp.push_back(std::move(*rbegin++));
	}

	std::move(begin, mid, std::back_inserter(tmp));
	std::move(rbegin, end, std::back_inserter(tmp));

	return tmp;
}

// Based off a project I wrote many years ago (with a few improvements):
// https://github.com/aryan-gupta/VisualSorting/blob/develop/MergeSort.h
// We want to check if the iterator is at least a fwd iterator. this algo
// doesnt work with anything less
template <typename I, typename O = std::greater<typename I::value_type>,
          typename = std::enable_if_t<std::is_base_of<std::forward_iterator_tag, typename std::iterator_traits<I>::iterator_category>::value>>
void merge_sort(I begin, I end, O op = {  }) {
	auto size = std::distance(begin, end);
	if (size <= 1)
		return;

	auto mid = std::next(begin, size / 2);
	merge_sort(begin, mid, op);
	merge_sort(mid, end, op);

	auto merged = merge_sort_merge(begin, mid, end, op);
	std::move(merged.begin(), merged.end(), begin);
}


int main() {
	std::vector<NoCopy> data;

	// I stole code from here: https://stackoverflow.com/questions/19665818
	// I will watch that video later to figure out what this exactally does
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_real_distribution<float> dist(1.0, 100'000.0);

	constexpr size_t MAX = 100'000;
	for (int i = 0; i < MAX; ++i) {
		data.push_back(NoCopy{ (int)dist(mt)} );
	}

	// for (int i = 0; i < MAX; ++i)
	// 	std::cout << data[i] << ", ";
	// std::cout << std::endl;

	merge_sort(data.begin(), data.end());
	// Haha its sorted in decending order, need to change this to reverse iterators
	if (std::is_sorted(data.rbegin(), data.rend()))
		std::cout << "Correct" << std::endl;

	merge_sort(data.begin(), data.end(), std::less<typename decltype(data)::value_type> {  });
	if (std::is_sorted(data.begin(), data.end()))
		std::cout << "Correct" << std::endl;

	// for (int i = 0; i < MAX; ++i)
	// 	std::cout << data[i] << ", ";
	// std::cout << std::endl;
}