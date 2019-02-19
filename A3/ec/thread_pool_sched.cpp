// WARNING: Because this is extra credit, I am not taking consideration with backwards compatiblity. Alot of the code
// here is cutting edge (mostly c++17 and a few c++20). I was able to compile this code with g++ (GCC) 8.2.1 20181127
// with the -std=c++17 option

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <cmath>
#include <chrono>
#include <utility>
#include <tuple>
#include <vector>
#include <memory>
#include <stdexcept>
#include <atomic>
#include <thread>
#include <stdint.h>
#include <mutex>
#include <queue>
#include <condition_variable>

#include "thread_pool.hpp"

using hrc = std::chrono::high_resolution_clock;


using func_t = float (*)(float, int);
using pthread_func_t = void* (*) (void*);


#ifdef __cplusplus
extern "C" {
#endif

float f1(float x, int intensity);
float f2(float x, int intensity);
float f3(float x, int intensity);
float f4(float x, int intensity);

#ifdef __cplusplus
}
#endif

namespace detail {

/// This class is the base so we can perform some time erasure
struct TPS_func_wrapper_base {
	virtual ~TPS_func_wrapper_base() {  };
	virtual void operator() () = 0;
};


/// This is the derived class of the base
template <typename T>
class TPS_func_wrapper : public TPS_func_wrapper_base {
	T mFunc;

public:
	TPS_func_wrapper(T func) : mFunc{ func } {  }

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
	TPS_callable(TPS_func_wrapper_base* func) : mFunc{ func } {  }

	~TPS_callable() {
		delete mFunc;
	}

	void operator() () {
		mFunc->operator()();
	}
};

} // namespace detail


/// This class is a secdular, you can post functions on the schedular and
/// another thread can pull jobs from this
class ThreadPoolSchedular {
public:
	using callable_t = detail::TPS_callable;

private:
	using func_t = detail::TPS_func_wrapper_base*;
	using lock_t = std::unique_lock<std::mutex>;
	template <typename A> using derived_t = detail::TPS_func_wrapper<A>;

	std::queue<func_t> mQ;
	std::mutex mLock;
	std::condition_variable mSignal;

public:
	ThreadPoolSchedular() : mQ{  }, mLock{  }, mSignal{  } {}

	callable_t pop() {
		func_t func = nullptr;
		{
			lock_t lk{ mLock };
			if (mQ.empty())
				mSignal.wait(lk, [this](){ return !mQ.empty(); });
			func = mQ.front();
			mQ.pop();
		}
		return { func };
	}

	template <typename T>
	void push(T func) {
		func_t wrapper = new derived_t<T>{ func };
		{
			lock_t lk{ mLock };
			mQ.push(wrapper);
		}
		mSignal.notify_one();
	}

	bool empty() {
		lock_t lk{ mLock };
		return mQ.empty();
	}

};


std::mutex cout_mux{  };

enum class SYNC {
	ITERATE,
	// THREAD,
	CHUNK
};


std::atomic_bool gEndThread;


struct JobHolder {
	float answer;
	std::mutex lock;
	std::atomic_int left;

	JobHolder() : answer{ }, lock{  }, left{  } {  }

	bool done() {
		return left == 0;
	}
};

template <SYNC sync>
class integrate_work {
	func_t functionid;
	int a;
	int b;
	int n;
	int start;
	int end;
	int intensity;
	JobHolder* job;

public:
	integrate_work() = default;

	integrate_work(func_t func, int a, int b, int n, int start, int end, int intensity, JobHolder* holder)
		: functionid{ func }
		, a{ a }
		, b{ b }
		, n{ n }
		, start { start }
		, end{ end }
		, intensity{ intensity }
		, job{ holder }
		{}

	void sync_with_shared(float& ans) {
		std::unique_lock lk{ job->lock };
		job->answer += ans;
		ans = 0;
	}

	void operator() () {
		float ban = (b - a) / (float)n;
		float ans{};

		for (int i = start; i < end; ++i) {
			float x = a + ((float)i + 0.5) * ban;
			ans += functionid(x, intensity);
			if constexpr (sync == SYNC::ITERATE) sync_with_shared(ans);
		}
		if constexpr (sync == SYNC::CHUNK) sync_with_shared(ans);

		job->left.fetch_add(-1);
	}
};


void thread_work(ThreadPoolSchedular& sch) {
	while(true) {
		auto work = sch.pop();
		work();
	}
}


void start_threads(ThreadPoolSchedular& sch, size_t num) {
	while (num --> 0) {
		std::thread th{ thread_work, std::ref(sch) };
		th.detach();
	}
}


// returns the current number of jobs compleated and the number of total jobs it is enqued
JobHolder* submit_jobs(ThreadPoolSchedular& tps, func_t functionid, int a, int b, int n, int intensity, int depth, SYNC sync) {
	JobHolder* jh = new JobHolder{  };

	int start = 0;
	int end = depth;


	while (start + depth < n) {
		jh->left.fetch_add(1);
		if (sync == SYNC::ITERATE) {
			integrate_work<SYNC::ITERATE> iw{ functionid, a, b, n, start, end, intensity, jh };
			tps.push(iw);
		} else {
			integrate_work<SYNC::CHUNK> iw{ functionid, a, b, n, start, end, intensity, jh };
			tps.push(iw);
		}

		start = end;
		end += depth;
	}

	jh->left.fetch_add(1);
	if (sync == SYNC::ITERATE) {
		integrate_work<SYNC::ITERATE> iw{ functionid, a, b, n, start, n, intensity, jh };
		tps.push(iw);
	} else {
		integrate_work<SYNC::CHUNK> iw{ functionid, a, b, n, start, n, intensity, jh };
		tps.push(iw);
	}

	// return std::unique_ptr<JobHolder>{ jh };
	return jh;
}


int main (int argc, char* argv[]) {

	std::vector<JobHolder*> jobs{  };

	std::cout << "Please choose the number of threads to start..." << std::endl;
	std::cout << ":: ";
	unsigned nbthreads{  };

	do {
		int tmp;
		std::cin >> tmp;
		if (tmp > 0) {
			nbthreads = tmp;
		} else {
			std::cout << "That is incorrect... Try again\n:: ";
			std::cin.clear();
			std::cin.ignore(SIZE_MAX);
		}
	} while(nbthreads == 0);

	ThreadPoolSchedular tps{  };
	start_threads(tps, nbthreads);
	std::cout << "Started " << nbthreads << " threads" << std::endl;

	std::cout << "What function would you like to integrate?" << std::endl;
	std::cout << ":: ";
	unsigned short id;
	std::cin >> id;

	func_t func = nullptr;
	switch (id) {
		case 1: func = f1; break;
		case 2: func = f2; break;
		case 3: func = f3; break;
		case 4: func = f4; break;
		default: {
			std::cout << "[E] Ya, you screwed up... have fun" << std::endl;
			return -2;
		}
	}

	std::cout << "What is the start of integration?" << std::endl;
	std::cout << ":: ";
	int a;
	std::cin >> a;

	std::cout << "What is the end of integration?" << std::endl;
	std::cout << ":: ";
	int b;
	std::cin >> b;

	std::cout << "What is the width of dx?" << std::endl;
	std::cout << ":: ";
	int n;
	std::cin >> n;

	std::cout << "What is the intensity?" << std::endl;
	std::cout << ":: ";
	int intensity;
	std::cin >> intensity;

	std::cout << "What is the granulity of work division?" << std::endl;
	std::cout << ":: ";
	int gran;
	std::cin >> gran;

	std::cout << "Calculating..." << std::endl;

	auto handle = submit_jobs(tps, func, a, b, n, intensity, gran, SYNC::ITERATE);


	while (!handle->done());

	float ans = handle->answer * (b - a) / (float)n;

	std::cout << "Answer is: " << ans << std::endl;

	return 0;
}
