
#include <memory>
#include <iostream>
#include <vector>
#include <thread>
#include <cassert>

constexpr unsigned long long LG_NUM = 200'000'000;

template <typename T>
void par_sum (T* start, T* end, T* dest) {
	*dest = *start;
	while (++start != end) {
		*dest += *start;
	}
}

template <typename T>
T reduce_sin(T* array, size_t n, size_t p = 1) {
	assert(p == 1);

	// create nessicary structures
	std::unique_ptr<T[]> data{ new T[p] };

	// calculate parallelism
	size_t depth = n / p;
	T* start = array;
	T* end = array + depth;

	// the actual parallel code
	for (size_t i = 0; i < p - 1; ++i) {
		par_sum(start, end, &data[i]);
		start = end;
		end += depth;
	}

	// the last one
	par_sum(start, array + n, &data[p - 1]);

	// final sum
	T sum{  };
	for (size_t i = 0; i < p; ++i)
		sum += data[i];

	return sum;
}

template <typename T>
T reduce_par (T* array, size_t n, size_t p) {
	// create nessicary structures
	std::unique_ptr<T[]> data{ new T[p] };
	std::vector<std::thread> threads{  };

	// calculate parallelism
	size_t depth = n / p;
	T* start = array;
	T* end = array + depth;

	// the actual parallel code
	for (size_t i = 0; i < p - 1; ++i) {
		// The <T> is needed because par_sum is a templated function so we need
		// to thread the <T> version of the function
		threads.emplace_back(par_sum<T>, start, end, &data[i]);
		start = end;
		end += depth;
	}

	// the last one
	threads.emplace_back(par_sum<T>, start, array + n, &data[p - 1]);

	// wait for them to finish
	for (auto& t : threads)
		t.join();

	// final sum
	T sum{  };
	for (size_t i = 0; i < p; ++i)
		sum += data[i];

	return sum;
}


int main() {
	std::vector<unsigned long long> sum{};
	for (unsigned long long i = 1; i < LG_NUM + 1; ++i) {
		sum.push_back(i);
	}

	auto begin1  = std::chrono::high_resolution_clock::now(); //// AHHHHHHH
	long accum1  = reduce_sin(sum.data(), LG_NUM, 1);
	auto end1    = std::chrono::high_resolution_clock::now(); //// AHHHHHHH

	std::chrono::duration<double, std::milli> elapse1{ end1 - begin1 };
	std::cout << "Serial: " << elapse1.count() << std::endl;

	auto begin2  = std::chrono::high_resolution_clock::now(); //// AHHHHHHH
	long accum2  = reduce_par(sum.data(), LG_NUM, 8);
	auto end2    = std::chrono::high_resolution_clock::now(); //// AHHHHHHH

	std::chrono::duration<double, std::milli> elapse2{ end2 - begin2 };
	std::cout << "Parallel: " << elapse2.count() << std::endl;

	if (accum1 == accum2)
		std::cout << "The sum is: " << accum1 << std::endl;
	else
		std::cout << "The sums were different, you screwed up..." << std::endl;
}