#pragma once

#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>

#include "lockfree_queue.hpp"
#include "parallel_queue.hpp"

// Internal implementation for the ThreadPoolSchedular. I wanted to make the code as modular as possible
// so I took some inspiration from std::any. The gist of this code is that there is a base class with a v-table
// for operator(), there is a templated derived class called TPS_func_wrapper. Becuase this class is templated,
// we can erase the type of the template because the only thing we want to do with this is call the callable.
// To farther abstract the internals, I created a TPS_callable that will make sure that the memory gets properly
// released and the user can call an object rather than call a pointer to the object.
namespace detail {

/// This class is the base so we can perform some time erasure
struct TPS_func_wrapper_base {
	virtual ~TPS_func_wrapper_base() {  };
	virtual void operator() () = 0;
};


/// This is the derived class of the base. Because this class is templated
/// every version of this code will derive from the base. That means we will have
/// one base class and many types of derived class (one for each type erasure). This
/// way it doesnt matter what T is, as long as its a callable, we can call it.
template <typename T>
class TPS_func_wrapper : public TPS_func_wrapper_base {
	T mFunc;

public:
	TPS_func_wrapper(T&& func) : mFunc{ std::forward<T>(func) } {  }

	virtual ~TPS_func_wrapper() = default;

	virtual void operator() () {
		mFunc();
	}
};

/// Class to store and release the memory of a callable
class TPS_callable {
	TPS_func_wrapper_base* mFunc;

public:
	TPS_callable() = delete;
	TPS_callable(const TPS_callable&) = delete;
	TPS_callable(TPS_callable&& o) : mFunc{ o.mFunc } {
		o.mFunc = nullptr;
	}

	TPS_callable(TPS_func_wrapper_base* func) : mFunc{ func } {  }

	~TPS_callable() {
		if (mFunc != nullptr)
			delete mFunc;
	}

	void operator() () {
		if (mFunc != nullptr)
			mFunc->operator()();
	}
};

} // namespace detail


/// This class is the thread pool schedular, you can post functions on the schedular and
/// another thread can pull jobs from this and compleate the job. The job can be any callable
/// that does not return anything or take anything as a parameter. This means callable objects,
/// lambda functions, plain function pointers and std::bind are acceptable. This Schedular
/// also takes care of ending the threads when we are done with the program.
// @todo Allow the user to call the callable with their own variac parameters
class ThreadPoolSchedular {
public:
	using callable_t = detail::TPS_callable;

private:
	using func_t = detail::TPS_func_wrapper_base*;
	using lock_t = std::unique_lock<std::mutex>;
	template <typename A> using derived_t = detail::TPS_func_wrapper<A>;

	std::vector<std::thread> mThreads;

	parallel_queue<func_t> mQ;
	std::atomic_bool mKill;

public:
	ThreadPoolSchedular() = delete;

	ThreadPoolSchedular(int nbthreads) : mQ{  }, mKill{ false } {
		while(nbthreads --> 0) {
			mThreads.emplace_back(thread_loop, std::ref(*this));
		}
	}

	static void thread_loop(ThreadPoolSchedular& tps) {
		while(!tps.mKill.load()) {
			auto [cont, work] = tps.pop();
			if (cont) work();
			else break;
		}
	}

	template <typename T>
	static void master_loop(ThreadPoolSchedular& tps, T* handle) {
		while (!handle->done) {
			auto [cont, work] = tps.try_pop();
			if (cont) work();
		}
	}

	~ThreadPoolSchedular() {
		end();
		for(auto& t : mThreads) {
			if (t.joinable())
				t.join();
		}
	}

	std::pair<bool, callable_t> pop() {
		auto [success, func] = mQ.try_pop( [this](){ return !mKill.load(); } );
		return { success, callable_t{ func } };
	}

	std::pair<bool, callable_t> try_pop() {
		auto [success, func] = mQ.try_pop();
		return { success, callable_t{ func } };
	}

	template <typename T>
	void push(T&& func) {
		func_t wrapper = new derived_t<T>{ std::forward<T>(func) };
		mQ.push(wrapper);
	}

	void end() {
		mKill = true;
	}

};