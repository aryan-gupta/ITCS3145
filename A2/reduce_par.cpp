
#include <memory>
#include <iostream>
#include <vector>
#include <thread>
#include <cassert>
// #include <numeric>
// #include <execution>
#include <parallel/numeric>

constexpr unsigned long long LG_NUM = 500'000'007;

/// @warning I know there wont be any race conditions on `data`
///          variable because it is unique to each thread.
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
	T result = array[0];
	for (int i=1; i<n; ++i)
		result = [](auto a, auto b) { return a + b; }(result, array[i]);
	return result;
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

	// the last one (this thread will do it so we dont spin on waiting for them to join)
	par_sum(start, array + n, &data[p - 1]);

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
	std::vector<int> sum{};
	for (unsigned long long i = 1; i < LG_NUM + 1; ++i) {
		sum.push_back(1); // sums of ones
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

	// Turns out that neither gnu g++ or clang++ supports the parallelism TS, kms
	// https://youtu.be/Mcjrc2uxbKI?t=571 MSVC ahead of the game *slow claps*
	// nvm: https://godbolt.org/z/TCGaze The internet video lies
	// auto begin3 = std::chrono::high_resolution_clock::now();
	// double result = std::reduce(std::execution::par, sum.begin(), sum.end());
	// auto end3 = std::chrono::high_resolution_clock::now();

	// std::chrono::duration<double, std::milli> elapse3{ end3 - begin3 };
	// std::cout << "STL: " << elapse3.count();

	// hehe https://gcc.gnu.org/onlinedocs/libstdc++/manual/parallel_mode.html
	auto begin4 = std::chrono::high_resolution_clock::now();
	double accum4 = __gnu_parallel::accumulate(sum.begin(), sum.end(), 0);
	auto end4 = std::chrono::high_resolution_clock::now();

	std::chrono::duration<double, std::milli> elapse4{ end4 - begin4 };
	std::cout << "GNU: " << elapse4.count() << std::endl;

	if (accum1 == accum2 and accum2 == accum4)
		std::cout << "The sum is: " << accum1 << std::endl;
	else
		std::cout << "The sums were different, you screwed up..." << std::endl;
}