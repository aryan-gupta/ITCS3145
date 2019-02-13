// This code is extra credit and I refuse to use pthread more then I need to. C++11 threads
// are more fun and more type safe

#include <vector>
#include <random>
#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>


using hrc = std::chrono::high_resolution_clock;

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

namespace detail {
// No need for SFINE here because it will only be called by merge_sort, meaning the types will be corect
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
}

namespace serial {
// Based off a project I wrote many years ago (with a few improvements):
// https://github.com/aryan-gupta/VisualSorting/blob/develop/MergeSort.h
// We want to check if the iterator is at least a fwd iterator. this algo
// doesnt work with anything less
template <typename I, typename O = std::less<typename I::value_type>,
          typename = std::enable_if_t<std::is_base_of<std::forward_iterator_tag, typename std::iterator_traits<I>::iterator_category>::value>>
void merge_sort(I begin, I end, O op = {  }) {
	auto size = std::distance(begin, end);
	if (size <= 1)
		return;

	auto mid = std::next(begin, size / 2);
	merge_sort(begin, mid, op);
	merge_sort(mid, end, op);

	auto merged = ::detail::merge_sort_merge(begin, mid, end, op);
	std::move(merged.begin(), merged.end(), begin);
}
}

namespace parallel_static {

// @todo add some parallelism to parallel_merge function

template <typename I, typename O>
void merge_sort_parallel_merge(I begin, I end, unsigned nbt, O op = {  }) {
	if (nbt <= 1) return;

	auto size = std::distance(begin, end);
	auto mid = std::next(begin, size / 2);

	merge_sort_parallel_merge(begin, mid, nbt / 2, op);
	merge_sort_parallel_merge(mid, end, nbt / 2, op);

	auto merged = ::detail::merge_sort_merge(begin, mid, end, op);
	std::move(merged.begin(), merged.end(), begin);
}

// if a number is a power of 2 then it will be in a format of 10000...
// this will translate to (1000 & 0111) == 0
bool is_pow_2(unsigned long num) {
	return (num & (num - 1)) == 0;
}

template <typename I, typename O = std::less<typename I::value_type>,
          typename = std::enable_if_t<std::is_base_of<std::forward_iterator_tag, typename std::iterator_traits<I>::iterator_category>::value>>
void merge_sort(I begin, I end, unsigned nbt, O op = {  }) {
	// we need to make sure that nbt is a power of 2
	if (!is_pow_2(nbt))
		return; // @todo throw here

	std::vector<std::thread> thread_storage{  };

	auto size = std::distance(begin, end);
	size_t depth = size / nbt;

	auto st = begin;
	auto en = std::next(st, depth);

	for (int i = 0; i < nbt; ++i) {
		thread_storage.emplace_back(serial::merge_sort<I, O>, st, en, op);
		st = en;
		en = std::next(st, depth);
	}

	for (auto& t : thread_storage)
		t.join();

	// here is the fun part. The structure is now split into nbt parts that are
	// each sorted, now we have to do merges on each 2 of the parts until we have 1 large one
	// left. reverse the algorithm of merge sort. Im unsure of this algo but Im going to
	// try it. It works, but we do the recurse again
	merge_sort_parallel_merge(begin, end, nbt, op);
}
}


auto create_array_to_sort() {
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

	return data;
}

int main() {
{
	auto sort_data = create_array_to_sort();
	auto timeStart = hrc::now();
	parallel_static::merge_sort(sort_data.begin(), sort_data.end(), 8);
	auto timeEnd = hrc::now();
	std::chrono::duration<double> elapse{ timeEnd - timeStart };
	float time = elapse.count();
	if (std::is_sorted(sort_data.begin(), sort_data.end()))
		std::cout << "Correct, took " << time << std::endl;
}

{
	auto sort_data = create_array_to_sort();
	auto timeStart = hrc::now();
	serial::merge_sort(sort_data.begin(), sort_data.end());
	auto timeEnd = hrc::now();
	std::chrono::duration<double> elapse{ timeEnd - timeStart };
	float time = elapse.count();
	if (std::is_sorted(sort_data.begin(), sort_data.end()))
		std::cout << "Correct, took " << time << std::endl;
}

}