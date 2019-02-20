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
		std::cout << begin->get() << " ";
	}
	std::cout << "\n";

}


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

	int get() const {
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

namespace parallel_threadpool {

struct JobHandle {
	std::atomic_bool done;
};

template <typename I, typename O>
struct MergeSortWork {
	I mBegin;
	I mMid;
	I mEnd;
	O mOp;
	JobHandle* mLeft;
	JobHandle* mRight;
	JobHandle* mHandle;

public:
	MergeSortWork() = default;
	// @todo Make this code so we reduce the amount of copies
	MergeSortWork(MergeSortWork& ) = delete;
	MergeSortWork(MergeSortWork&& ) = default;

	MergeSortWork(I begin, I mid, I end, O op, JobHandle* left, JobHandle* right, JobHandle* mine)
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
JobHandle* merge_sort_threadpool_sort(ThreadPoolSchedular& tps, I begin, I end, O op) {
	std::deque<std::tuple<I, I, JobHandle*>> jobs{  };
	JobHandle* alwaysTrue = new JobHandle{ true };

	for (I it{ begin }; it != end; ) {
		I s = it++;
		I m = it++;
		if (m == end) {
			jobs.emplace_back(s, m, alwaysTrue);
			break;
		}
		I e = it;

		JobHandle* handle = new JobHandle{ false };
		tps.push(MergeSortWork<I, O>{ s, m, e, op, alwaysTrue, alwaysTrue, handle });
		jobs.emplace_back(s, e, handle);
	}

	JobHandle* handle;
	while (jobs.size() > 1) {
		auto [s, m1, left] = jobs.front(); jobs.pop_front();
		auto [m2, e, right] = jobs.front();

		if (m1 == m2) {
			handle = new JobHandle{ false };
			tps.push(MergeSortWork<I, O>{ s, m1, e, op, left, right, handle });
			jobs.emplace_back(s, e, handle);
			jobs.pop_front();
		} else {
			jobs.emplace_back(s, m1, left);
		}
	}
	return std::get<2>(jobs.front());
}

void thread_work(ThreadPoolSchedular& tps, std::atomic_uint8_t& num) {
	num.fetch_add(1);
	while(true) {
		auto [cont, work] = tps.pop();
		if (cont) work();
		else break;
	}
	num.fetch_add(-1);
}

template <typename I, typename O = std::less<typename I::value_type>,
          typename = std::enable_if_t<std::is_base_of<std::forward_iterator_tag, typename std::iterator_traits<I>::iterator_category>::value>>
void merge_sort(I begin, I end, unsigned nbthreads, O op = {  }) {
	// Start up threads
	/// @todo add a way to pass in threads that have alread been started
	ThreadPoolSchedular tps{  };
	std::atomic_uint8_t numThreads{ 0 };
	while(nbthreads --> 0) std::thread{ thread_work, std::ref(tps), std::ref(numThreads) }.detach();

	auto master_handle = merge_sort_threadpool_sort(tps, begin, end, op);

	while (!master_handle->done) {
		auto [cont, work] = tps.try_pop();
		if (cont) work();
	}
	tps.end();
	while (numThreads != 0);
	return;
}
}

// attribute((no_instrument_function))
auto create_array_to_sort(int MAX = 100'007) {
	std::vector<NoCopy> data;

	// I stole code from here: https://stackoverflow.com/questions/19665818
	// I will watch that video later to figure out what this exactally does
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_real_distribution<float> dist(1.0, MAX);

	for (int i = 0; i < MAX; ++i) {
		data.push_back(NoCopy{ (int)dist(mt)} );
	}

	return data;
}

int main() {
	// for (int i = 1; i < 50; ++i)
{
	auto sort_data = create_array_to_sort();
	// print(sort_data.begin(), sort_data.end());
	auto timeStart = clk::now();
	parallel_threadpool::merge_sort(sort_data.begin(), sort_data.end(), 4);
	auto timeEnd = clk::now();
	// print(sort_data.begin(), sort_data.end());
	std::chrono::duration<double> elapse{ timeEnd - timeStart };
	float time = elapse.count();
	if (std::is_sorted(sort_data.begin(), sort_data.end())) {
		std::unique_lock{ out_mux };
		std::cout << "Threadpool took " << time << std::endl;
	}
}
{
	auto sort_data = create_array_to_sort();
//	print(sort_data.begin(), sort_data.end());
	auto timeStart = clk::now();
	parallel_static::merge_sort(sort_data.begin(), sort_data.end(), 8);
	auto timeEnd = clk::now();
//	print(sort_data.begin(), sort_data.end());
	std::chrono::duration<double> elapse{ timeEnd - timeStart };
	float time = elapse.count();
	if (std::is_sorted(sort_data.begin(), sort_data.end()))
		std::cout << "Parallel   took " << time << std::endl;
}

{
	auto sort_data = create_array_to_sort();
//	print(sort_data.begin(), sort_data.end());
	auto timeStart = clk::now();
	serial::merge_sort(sort_data.begin(), sort_data.end());
	auto timeEnd = clk::now();
//	print(sort_data.begin(), sort_data.end());
	std::chrono::duration<double> elapse{ timeEnd - timeStart };
	float time = elapse.count();
	if (std::is_sorted(sort_data.begin(), sort_data.end()))
		std::cout << "Serial     took " << time << std::endl;
}

}