// This code is extra credit and I refuse to use pthread more then I need to. C++11 threads
// are more fun and more type safe

#include <vector>
#include <random>
#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>

#include "thread_pool.hpp"

using clk = std::chrono::high_resolution_clock;
static std::chrono::duration<double> gBusyTime{ 0 };
static std::atomic_int gBusyLoops{ 0 };
static std::mutex out_mux;


template <typename I>
void print(I begin, I end) {
	std::unique_lock lk { out_mux };
	for (; begin != end; ++begin) {
		std::cout << (int)begin->get() << " ";
	}
	std::cout << "\n";

}


// This class serves the purpose of testing that there are no useless
// copies of data. The copy functions are deleted meaning there when an
// object is constructed, it cannot be copied, only moved
template <typename T>
class NoCopy {
	T data;

public:
	NoCopy() = default;
	NoCopy(T d) : data{ d } {  };

	NoCopy(NoCopy&) = delete;
	NoCopy& operator= (NoCopy&) = delete;

	/// WE CAN MOVE THIS
	NoCopy(NoCopy&&) = default;
	NoCopy& operator= (NoCopy&&) = default;

	T get() const {
		return data;
	}

	// here we goooooo........
	// well thats great no default compare operators until c++20
	bool operator<  (const NoCopy& o) const { return data <  o.data; }
	bool operator<= (const NoCopy& o) const { return data <= o.data; }
	bool operator>  (const NoCopy& o) const { return data >  o.data; }
	bool operator>= (const NoCopy& o) const { return data >= o.data; }
	bool operator== (const NoCopy& o) const { return data == o.data; }
	bool operator!= (const NoCopy& o) const { return data != o.data; }

	template <typename U> friend std::ostream& operator<< (std::ostream&, const NoCopy<U>&);
};

template <typename T>
std::ostream& operator<< (std::ostream& out, const NoCopy<T>& data) {
	out << data;
	return out;
}


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

	for (size_t i = 0; i < nbt - 1; ++i) {
		thread_storage.emplace_back(serial::merge_sort<I, O>, st, en, op);
		st = en;
		en = std::next(st, depth);
	}

	thread_storage.emplace_back(serial::merge_sort<I, O>, st, end, op);

	for (auto& t : thread_storage)
		t.join();

	// here is the fun part. The structure is now split into nbt parts that are
	// each sorted, now we have to do merges on each 2 of the parts until we have 1 large one
	// left. reverse the algorithm of merge sort. Im unsure of this algo but Im going to
	// try it. It works, but we do the recurse again
	merge_sort_parallel_merge(begin, end, nbt, op);
}
}

namespace parallel_threadpool {

struct JobHandle {
	std::atomic_bool done;
};

template <typename I, typename O>
struct MergeSortMergeWork {
	I mBegin;
	I mMid;
	I mEnd;
	O mOp;
	JobHandle* mLeft;
	JobHandle* mRight;
	JobHandle* mHandle;

public:
	MergeSortMergeWork() = default;
	// @todo Make this code so we reduce the amount of copies
	MergeSortMergeWork(MergeSortMergeWork& ) = delete;
	MergeSortMergeWork(MergeSortMergeWork&& ) = default;

	MergeSortMergeWork(I begin, I mid, I end, O op, JobHandle* left, JobHandle* right, JobHandle* mine)
		: mBegin{ begin }
		, mMid{ mid }
		, mEnd{ end }
		, mOp{ op }
		, mLeft{ left }
		, mRight{ right }
		, mHandle{ mine }
		{}

	void operator() () {
		// @todo see how much time we waste here
		// or spinning for more work to do
		while (!mLeft->done or !mRight->done); // wait until the parent two nodes are sorted
		auto merged = ::detail::merge_sort_merge(mBegin, mMid, mEnd, mOp);
		std::move(merged.begin(), merged.end(), mBegin);
		// print(mBegin, mEnd);
		mHandle->done = true;
	}
};

template <typename I, typename O>
struct MergeSortWork {
	I mBegin;
	I mEnd;
	O mOp;
	JobHandle* mHandle;

public:
	MergeSortWork() = default;
	// @todo Make this code so we reduce the amount of copies
	MergeSortWork(MergeSortWork& ) = delete;
	MergeSortWork(MergeSortWork&& ) = default;

	MergeSortWork(I begin, I end, O op, JobHandle* mine)
		: mBegin{ begin }
		, mEnd{ end }
		, mOp{ op }
		, mHandle{ mine }
		{}

	void operator() () {
		::serial::merge_sort(mBegin, mEnd, mOp);
		mHandle->done = true;
	}
};

template <typename I, typename O>
JobHandle* merge_sort_threadpool_sort(ThreadPoolSchedular& tps, I begin, I end, O op) {
	std::deque<std::tuple<I, I, JobHandle*>> jobs{  };
	I it{ begin };

	constexpr size_t gran = 1;

	for (; it < end - gran; it += gran) {
		JobHandle* handle = new JobHandle{ false };
		tps.post(MergeSortWork<I, O>{ it, it + gran, op, handle });
		jobs.emplace_back(it, it + gran, handle);
	}

	JobHandle* handle = new JobHandle{ false };
	tps.post(MergeSortWork<I, O>{ it, end, op, handle });
	jobs.emplace_back(it, end, handle);

	while (jobs.size() > 1) {
		auto [s, m1, left] = jobs.front(); jobs.pop_front();
		auto [m2, e, right] = jobs.front();

		if (m1 == m2) {
			handle = new JobHandle{ false };
			tps.post(MergeSortMergeWork<I, O>{ s, m1, e, op, left, right, handle });
			jobs.emplace_back(s, e, handle);
			jobs.pop_front();
		} else {
			jobs.emplace_back(s, m1, left);
		}
	}
	return std::get<2>(jobs.front());
}

template <typename I, typename O = std::less<typename I::value_type>,
          typename = std::enable_if_t<std::is_base_of<std::forward_iterator_tag, typename std::iterator_traits<I>::iterator_category>::value>>
void merge_sort(I begin, I end, ThreadPoolSchedular& tps, O op = {  }) {
	auto master_handle = merge_sort_threadpool_sort(tps, begin, end, op);
	while (!master_handle->done.load()) {
		std::this_thread::yield();
	}
}

} // end namespace parallel_threadpool

// attribute((no_instrument_function))
auto create_array_to_sort(int MAX = 107) {
	std::vector<char> data;
	data.reserve(MAX * 1.5);

	// I stole code from here: https://stackoverflow.com/questions/19665818
	// I will watch that video later to figure out what this exactally does
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_real_distribution<float> dist(1.0, 255);

	for (int i = 0; i < MAX; ++i) {
		data.push_back((char)dist(mt) );
		// data.push_back(NoCopy{ i } );
	}

	return data;
}

/// Measures the exec time of a function and returns the result and time in seconds
/// @param func The function to call
/// @param args The arguments to pass into the function
/// @return A std::pair containing the time in seconds and the result
template <typename F, typename... A>
auto measure_func(F func, A... args) -> std::pair<float, typename std::result_of<F>::type> {
  auto start = clk::now();
  auto result = func( std::forward<A>(args)... );
  auto end = clk::now();
  std::chrono::duration<float> elapse = end - start;
  return { elapse.count(), result };
}


/// Measures the exec time of a function
/// @param func The function to call
/// @param args The arguments to pass into the function
/// @return The execution time in seconds
template<typename F, typename... A>
float measure_func(F func, A... args) {
  auto start = clk::now();
  func( std::forward<A>(args)... );
  auto end = clk::now();
  std::chrono::duration<float> elapse = end - start;
  return elapse.count();
}


int main() {
	{
		ThreadPoolSchedular tps{ 7 };
		auto data = create_array_to_sort();
		float elapse = measure_func([&](int){ parallel_threadpool::merge_sort(data.begin(), data.end(), tps); }, 0);
		if (std::is_sorted(data.begin(), data.end()))
			std::cout << "Threadpool took " << elapse << std::endl;
		tps.join();
	}

	{
		auto data = create_array_to_sort();
		float elapse = measure_func([&](int){ serial::merge_sort(data.begin(), data.end()); }, 0);
		if (std::is_sorted(data.begin(), data.end()))
			std::cout << "Serial took " << elapse << std::endl;
	}
}