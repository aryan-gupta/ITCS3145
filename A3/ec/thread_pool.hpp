#pragma once

#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>

#define USE_BOOST

#ifdef USE_BOOST
#include <boost/lockfree/queue.hpp>
#endif

#include "lockfree_queue.hpp"

// Internal implementation for the ThreadPoolSchedular. I wanted to make the code as modular as possible
// so I took some inspiration from std::any. The gist of this code is that there is a base class with a v-table
// for operator(), there is a templated derived class called tps_func_wrapper. Becuase this class is templated,
// we can erase the type of the template because the only thing we want to do with this is call the callable.
// To farther abstract the internals, I created a tps_callable that will make sure that the memory gets properly
// released and the user can call an object rather than call a pointer to the object.
namespace detail {

/// This class is the base so we can perform some time erasure
struct tps_func_wrapper_base {
	virtual ~tps_func_wrapper_base() = default;
	virtual void operator() () = 0;
};


/// This is the derived class of the base. Because this class is templated
/// every version of this code will derive from the base. That means we will have
/// one base class and many types of derived class (one for each type erasure). This
/// way it doesnt matter what T is, as long as its a callable, we can call it.
template <typename T>
class tps_func_wrapper : public tps_func_wrapper_base {
	T mFunc;

public:
	tps_func_wrapper(T&& func) : mFunc{ std::forward<T>(func) } {  }

	virtual ~tps_func_wrapper() = default;

	virtual void operator() () {
		mFunc();
	}
};


/// Class to store and release the memory of a callable
class tps_callable {
	tps_func_wrapper_base* mFunc;

public:
	tps_callable() = delete;
	tps_callable(const tps_callable&) = delete;
	tps_callable(tps_callable&& o) : mFunc{ o.mFunc } {
		o.mFunc = nullptr;
	}

	tps_callable(tps_func_wrapper_base* func) : mFunc{ func } {  }

	~tps_callable() {
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
	using callable_t = detail::tps_callable;
	using func_t = detail::tps_func_wrapper_base*;
	/// This type must have try_pop - MUST be wait free, IT CANNOT BLOCK or that thread waiting will wait forever if we need to end it
	/// and must have push
#ifdef USE_BOOST
	template <typename T> using queue_t = boost::lockfree::queue<T>;
#else
	template <typename T> using queue_t = ari::lockfree_queue<T>;
#endif
	template <typename A> using derived_t = detail::tps_func_wrapper<A>;

	std::vector<std::thread> mThreads;
	queue_t<func_t> mQ;
	std::atomic_bool mKill;

	std::pair<bool, callable_t> pop() {
		// Continue to try pop until we are successful. If we need to end the threads then tell the
		// theads to stop
		while (true) {
			if (mKill.load()) return { false, callable_t{ nullptr } };
		#ifdef USE_BOOST
			func_t func{  };
			bool success = mQ.pop(func);
		#else
			auto [success, func] = mQ.try_pop();
		#endif
			if (success) return { true, callable_t{ func } };
		}
	}

	static void thread_loop(ThreadPoolSchedular& tps) {
		while(true) {
			auto [cont, work] = tps.pop();
			if (cont) work();
			else break;
		}
	}


public:
	ThreadPoolSchedular() = delete;

	ThreadPoolSchedular(int nbthreads)
	#ifdef USE_BOOST
		: mQ{ 128 }
	#else
		: mQ{  }
	#endif
		, mKill{ false } {
		while(nbthreads --> 0) {
			mThreads.emplace_back(thread_loop, std::ref(*this));
		}
	}

	~ThreadPoolSchedular() {
		join();
	}

	template <typename T>
	void post(T&& func) {
		func_t wrapper = new derived_t<T>{ std::forward<T>(func) };
	#ifdef USE_BOOST
		while(!mQ.push(wrapper));
	#else
		mQ.push(wrapper);
	#endif
	}

	void join() {
		mKill = true;
		for(auto& t : mThreads) {
			if (t.joinable())
				t.join();
		}
	}

};