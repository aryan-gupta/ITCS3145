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

#include "thread_pool.hpp"

using hrc = std::chrono::high_resolution_clock;

std::mutex out_lock{  };

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

		{
			// std::lock_guard { out_lock };
			// std::cout << "Doing: " << start << "    " << end << std::endl;
		}

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
JobHolder* submit_jobs(ThreadPoolSchedular& tps, func_t functionid, int a, int b, int n, int intensity, int depth) {
	static int IDnum = 0;

	JobHolder* jh = new JobHolder{  };
	jh->id = IDnum++;

	int start = 0;
	int end = depth;


	while (start + depth < n) {
		{ std::lock_guard { out_lock };
			std::cout << "Pushing: " << start << "  " << end << std::endl;
		}
		jh->left.fetch_add(1);
		integrate_work iw{ functionid, a, b, n, start, end, intensity, jh };
		tps.push(iw);
		start = end;
		end += depth;
	}

	{ std::lock_guard { out_lock };
		std::cout << "Pushing: " << start << "  " << end << std::endl;
	}

	jh->left.fetch_add(1);
	integrate_work iw{ functionid, a, b, n, start, n, intensity, jh };
	tps.push(iw);

	return jh;
}


JobHolder* start_new_integration(ThreadPoolSchedular& tps) {
	std::cout << "What function would you like to integrate?" << std::endl;
	std::cout << ":: ";
	int id;
	std::cin >> id;

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

	std::cout << "What is the width of dx (n)?" << std::endl;
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


int main (int argc, char* argv[]) {

	std::vector<std::unique_ptr<JobHolder>> jobs{  };

	std::cout << "Please choose the number of threads to start..." << std::endl;
	std::cout << ":: ";
	unsigned nbthreads{ 4 };
	// std::cin >> nbthreads;

	ThreadPoolSchedular tps{ (int)nbthreads };
	std::cout << "Started " << nbthreads << " threads" << std::endl;

	unsigned choice{ 2 };
	do {
		std::cout << "What would you like to do?\n";
		std::cout << "0) Quit\n";
		std::cout << "1) Check Job Status\n";
		std::cout << "2) Start new integration\n";
		// std::cout << ":: ";
		// std::cin >> choice;
		switch (choice) {
			case 0: break;
			case 1: print_jobs(jobs); break;
			case 2: {
				auto handle = submit_jobs(tps, f1, 0, 10, 1'000, 1, 100); //start_new_integration(tps);
				std::lock_guard { out_lock };
				std::cout << "Job ID: " << handle->id << std::endl;
				while (!handle->done());
				std::cout << handle->answer << std::endl;
				while(1);
			} break;
		}
	} while (choice != 0);

	tps.end();
	// std::cout << "Waiting 5 seconds for detached threads to end..." << std::endl;
	// std::this_thread::sleep_for(std::chrono::seconds{ 5 });

	return 0;
}
