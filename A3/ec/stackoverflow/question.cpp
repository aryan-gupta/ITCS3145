// https://stackoverflow.com/questions/55140578/

#include <functional>
#include <cassert>

template <typename T>
T add (T a, T b) {
	return a + b;
}

template <typename T>
T sub (T a, T b) {
	return a - b;
}

/// The reason I am using something like this is because in my actual production code,
/// I have a measure_func function that calls a function and measures the time it takes
/// to execute that function. The function I am tring to measure has very long template
/// parameters and trying to get this might make getting the function pointer easier.
template <typename F, typename ...A>
std::invoke_result_t<F> do_operation(F func, A &&...args) {
	return func( std::forward<A>(args)... );
}

/// Attempt? Doesn't work tho
template<template <typename> typename T>
auto get_float_func(T t) {
	return &t<float>;
}


int main() {
	// here I can create a function or a templated struct that returns the address of add<float>
	auto float_add = get_float_func(add);

	// here get_float_func returns the address of sub<float>
	auto float_sub = get_float_func(sub);

	// that way I can more easily call do_operation like this
	float ans1 = do_operation(float_add, 1.5, 1.0);
	assert(ans1 == 2.5);
	float ans2 = do_operation(float_sub, 1.5, 1.0);
	assert(ans2 == 0.5);
}


//// EDIT
namespace serial {
	template <typename I, typename O = std::less<typename I::value_type>>
	void merge_sort(I begin, I end, O op = {  })
	{ /* code */ }
}

namespace parallel {
	template <typename I, typename O = std::less<typename I::value_type>>
	void merge_sort(I begin, I end, O op = {  })
	{ /* code */ }
}

namespace threadpool {
	template <typename I, typename O = std::less<typename I::value_type>>
	void merge_sort(I begin, I end, O op = {  })
	{ /* code */ }
}

int main() {
	std::vector<int> data = create_random_array();
	/// I want to create something that will always return the instantiation of merge_sort<std::vector<int>::iterator, std::less<int>>
	/// function pointer
	float elapse1 = measure_func(serial::merge_sort,     data.begin(), data.end(), std::less<int>{  });
	float elapse2 = measure_func(parallel::merge_sort,   data.begin(), data.end(), std::less<int>{  });
	float elapse3 = measure_func(threadpool::merge_sort, data.begin(), data.end(), std::less<int>{  });
}