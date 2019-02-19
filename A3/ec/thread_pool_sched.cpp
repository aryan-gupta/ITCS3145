// WARNING: Because this is extra credit, I am not taking consideration with backwards compatiblity. Alot of the code
// here is cutting edge (mostly c++17 and a few c++20). I was able to compile this code with g++ (GCC) 8.2.1 20181127
// with the -std=c++17 option. If I get the time I will add some PPD for pthreads
// I wont be using pthreads, if I do then I would have to replace all the mutex and cond_vars

// 4 2 1 0 10 10000 10000000 100 1 2 2 0 10 10000000 10 1000
// 4 2 1 0 10 10000 100000 100 1 2 2 0 10 10000000 10 1000

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

using hrc = std::chrono::high_resolution_clock;


using func_t = float (*)(float, int);
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
	TPS_callable() = delete; // dont want to deal with nullptr
	TPS_callable(TPS_func_wrapper_base* func) : mFunc{ func } {  }

	~TPS_callable() {
		delete mFunc;
	}

	void operator() () {
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

	// @todo make this lock-free? maybe
	std::queue<func_t> mQ;
	std::mutex mLock;
	std::condition_variable mSignal;

	std::atomic_bool mKill;

public:
	ThreadPoolSchedular() : mQ{  }, mLock{  }, mSignal{  }, mKill{ false } {}

	std::pair<bool, callable_t> pop() {
		func_t func = nullptr;
		{
			lock_t lk{ mLock };
			/// if its empty wait for more jobs or stop waiting if we eant to kill threads
			if (mQ.empty()) mSignal.wait(lk, [this](){ return !mQ.empty() or mKill; });
			if (mKill) return { false, nullptr };
			func = mQ.front();
			mQ.pop();
		}
		return { true, func };
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

	void end() {
		mKill = true;
		mSignal.notify_all();
	}

};


/// This holds information about a integration Job being run in the thead_pool. Keeps a count
/// of how many jobs are left to compleate before the answer can be ready. Also has a mutex to
/// protect the answer
struct JobHolder {
	int id;
	float answer;
	std::mutex lock;
	std::atomic_int left;

	JobHolder() = default;

	bool done() {
		return left == 0;
	}
};


/// This class holds a job that needs to be done when integrating. It stores the start and the end values
/// it is taking care of. For each integration there will be many of these classes. Each thread will pull
/// a work from the schedular, call the operator() on this and sync up with the shared variable.
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
		}
		sync_with_shared(ans);

		if (job->left == 1) {
			std::unique_lock lk{ job->lock };
			job->answer *= ban;
		}
		job->left.fetch_add(-1);
	}
};


void thread_work(ThreadPoolSchedular& sch) {
	while(true) {
		auto [cont, work] = sch.pop();
		if (cont) work();
		else return;
	}
}


void start_threads(ThreadPoolSchedular& sch, size_t num) {
	while (num --> 0) {
		std::thread th{ thread_work, std::ref(sch) };
		th.detach();
	}
}

/// This function breaks the integration into jobs that the threads can compleate. Then posts the jobs in the schedular
// returns the current number of jobs compleated and the number of total jobs it is enqued
std::unique_ptr<JobHolder> submit_jobs(ThreadPoolSchedular& tps, func_t functionid, int a, int b, int n, int intensity, int depth) {
	static int IDnum = 0;

	JobHolder* jh = new JobHolder{  };
	jh->id = IDnum++;

	int start = 0;
	int end = depth;


	while (start + depth < n) {
		jh->left.fetch_add(1);
		integrate_work iw{ functionid, a, b, n, start, end, intensity, jh };
		tps.push(iw);
		start = end;
		end += depth;
	}

	jh->left.fetch_add(1);
	integrate_work iw{ functionid, a, b, n, start, n, intensity, jh };
	tps.push(iw);

	return std::unique_ptr<JobHolder>{ jh };
}


std::unique_ptr<JobHolder> start_new_integration(ThreadPoolSchedular& tps) {
	std::cout << "What function would you like to integrate?" << std::endl;
	std::cout << ":: ";
	int id;
	std::cin >> id;
	std::cout << id;

	func_t func = nullptr;
	switch (id) {
		case 1: func = f1; break;
		case 2: func = f2; break;
		case 3: func = f3; break;
		case 4: func = f4; break;
		default: {
			std::cout << "[E] Ya, you screwed up... have fun" << std::endl;
			return nullptr;
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

	return submit_jobs(tps, func, a, b, n, intensity, gran);
}

void print_jobs(std::vector<std::unique_ptr<JobHolder>>& jobs) {
	for (auto& jhp : jobs) {
		std::cout << "ID: " << jhp->id << "\t\t";
		if (jhp->done())
			std::cout << "Answer: " << jhp->answer << std::endl;
		else
			std::cout << "Work is not done.... " << std::endl;
	}
}


#ifndef THREAD_POOL_AS_HEADER
int main (int argc, char* argv[]) {

	std::vector<std::unique_ptr<JobHolder>> jobs{  };

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
	} while (nbthreads == 0);

	ThreadPoolSchedular tps{  };
	start_threads(tps, nbthreads);
	std::cout << "Started " << nbthreads << " threads" << std::endl;

	unsigned choice;
	do {
		std::cout << "What would you like to do?\n";
		std::cout << "0) Quit\n";
		std::cout << "1) Check Job Status\n";
		std::cout << "2) Start new integration\n";
		std::cin >> choice;
		switch (choice) {
			case 0: break;
			case 1: print_jobs(jobs); break;
			case 2: {
				auto handle = start_new_integration(tps);
				std::cout << "Job ID: " << handle->id << std::endl;
				jobs.push_back(std::move(handle));
			} break;
		}
	} while (choice != 0);

	tps.end();
	std::cout << "Waiting 5 seconds for detached threads to end..." << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds{ 5 });

	return 0;
}
#endif