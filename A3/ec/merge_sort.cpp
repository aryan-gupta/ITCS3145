//  Just a word of warning, this code is very messy. One thing led to another and it just kept
// getting messier.

#include <vector>
#include <random>
#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>

#include "thread_pool.hpp"

using clk = std::chrono::high_resolution_clock;
static std::mutex out_mux;


// Print range between two iterator like objects
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
};



// Merge Sort merge
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



// Serial version of the merge sort
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



// parallel threadpooled version of mergesort
namespace parallel_threadpool {

// A handle for a posted job. Tells the depending jobs if this is done or not
struct JobHandle {
	std::atomic_bool done;
};

// A callable for merge sort merge
template <typename I, typename O>
struct MergeSortMergeWork {
	I mBegin;
	I mMid;
	I mEnd;
	O mOp;
	JobHandle* mLeft;
	JobHandle* mRight;
	JobHandle* mHandle;

	MergeSortMergeWork() = default;
	// @todo Make this code so we reduce the amount of copies
	MergeSortMergeWork(MergeSortMergeWork& ) = delete;
	MergeSortMergeWork(MergeSortMergeWork&& ) = default;

	void operator() () {
		// @todo see how much time we waste here
		// or spinning for more work to do
		while (!mLeft->done or !mRight->done); // wait until the parent two nodes are sorted
		auto merged = ::detail::merge_sort_merge(mBegin, mMid, mEnd, mOp);
		std::move(merged.begin(), merged.end(), mBegin);
		mHandle->done = true;
	}
};

// A callable for merge sort
template <typename I, typename O>
struct MergeSortWork {
	I mBegin;
	I mEnd;
	O mOp;
	JobHandle* mHandle;

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

	constexpr size_t gran = 1'000;

	for (; it < end - gran; it += gran) {
		JobHandle* handle = new JobHandle{ false };
		tps.push(MergeSortWork<I, O>{ it, it + gran, op, handle });
		jobs.emplace_back(it, it + gran, handle);
	}

	JobHandle* handle = new JobHandle{ false };
	tps.push(MergeSortWork<I, O>{ it, end, op, handle });
	jobs.emplace_back(it, end, handle);

	while (jobs.size() > 1) {
		auto [s, m1, left] = jobs.front(); jobs.pop_front();
		auto [m2, e, right] = jobs.front();

		if (m1 == m2) {
			handle = new JobHandle{ false };
			tps.push(MergeSortMergeWork<I, O>{ s, m1, e, op, left, right, handle });
			jobs.emplace_back(s, e, handle);
			jobs.pop_front();
		} else {
			jobs.emplace_back(s, m1, left);
		}
	}
	return std::get<2>(jobs.front());
}

template <typename I, typename O = std::less<typename I::value_type>,
          typename = std::enable_if_t<std::is_base_of<std::random_access_iterator_tag, typename std::iterator_traits<I>::iterator_category>::value>>
void merge_sort(I begin, I end, ThreadPoolSchedular& tps, O op = {  }) {
	auto master_handle = merge_sort_threadpool_sort(tps, begin, end, op);
	// ThreadPoolSchedular::master_loop(tps, master_handle);
	while (!master_handle->done) {
		std::this_thread::yield();
	}
}

} // end namespace parallel_threadpool

// attribute((no_instrument_function))
auto create_array_to_sort(int MAX = 100'000) {
	std::vector<NoCopy<char>> data;
	data.reserve(MAX * 1.5);

	// I stole code from here: https://stackoverflow.com/questions/19665818
	// I will watch that video later to figure out what this exactally does
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_real_distribution<float> dist(1.0, 255);

	for (int i = 0; i < MAX; ++i) {
		data.push_back(NoCopy{ (char)dist(mt) } );
		// data.push_back(NoCopy{ i } );
	}

	return data;
}

int main() {
// Threadpool merge sort
{
	ThreadPoolSchedular tps{ 8 };
	auto sort_data = create_array_to_sort();
	auto timeStart = clk::now();
	parallel_threadpool::merge_sort(sort_data.begin(), sort_data.end(), tps);
	auto timeEnd = clk::now();
	std::chrono::duration<double> elapse{ timeEnd - timeStart };
	float timeTaken = elapse.count();
	if (std::is_sorted(sort_data.begin(), sort_data.end()))
		std::cout << "Threadpool took " << timeTaken << std::endl;
}

// Serial sort
{
	auto sort_data = create_array_to_sort();
//	print(sort_data.begin(), sort_data.end());
	auto timeStart = clk::now();
	serial::merge_sort(sort_data.begin(), sort_data.end());
	auto timeEnd = clk::now();
//	print(sort_data.begin(), sort_data.end());
	std::chrono::duration<double> elapse{ timeEnd - timeStart };
	float timeTaken = elapse.count();
	if (std::is_sorted(sort_data.begin(), sort_data.end()))
		std::cout << "Serial     took " << timeTaken << std::endl;
}

}