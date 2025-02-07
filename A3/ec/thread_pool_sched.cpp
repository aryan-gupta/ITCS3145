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
#include <fstream>
#include <sstream>

#include "thread_pool.hpp"

using hrc = std::chrono::high_resolution_clock;

// Define to use cases file or manually submit jobs. Using the cases file
// usually leads this code to run in about 3-4 min using 7 threads
#define CASES_FILE

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


/// A JobHandle stores the information for one integrate job. The Job holds the
/// number of tasks left. Once there is no tasks left for one integration job, the
/// answer is ready. The answer is protected by a mutex and the ban must be calculated
/// at the time of creation of the job
class JobHandle {
	float answer;
	const float ban;
	std::mutex lock;
	std::atomic_uint32_t left;

public:
	JobHandle() = delete;
	JobHandle(float b) : answer{  }, ban{ b }, lock{  }, left{  } {  }

	/// Increase the number of tasks to finish this job
	void add() {
		left.fetch_add(1);
	}

	/// Decrease the number of tasks to finish this job
	void sub() {
		left.fetch_sub(1);
	}

	/// Syncs a task with the answer
	void sync(float local) {
		std::lock_guard lk { lock };
		answer += local;
	}

	/// returns if the Jobs is finished or not
	bool done() {
		return left == 0;
	}

	/// Recives the answer. Is undefined is the job is not done.
	/// @note not thread-safe
	float get() {
		return answer * ban;
	}
};


/// Partially integrates a function. Only integrates from start to finish of n values.
/// @param functionid The function to integrate
/// @param a The upper bound of the integral
/// @param b The lower bound of the integral
/// @param n An integer which is the number of points to compute the approximation of the integral
/// @param intensity An integer which is the second parameter to give the the function to integrate
/// @param start The start of \p n to integrate from
/// @param end The end of \p n to integrate too
/// @param jh The JobHandle for this job
void partial_integrate(func_t functionid, int a, int b, int n, int intensity, int start, int end, JobHandle* jh) {
	float ban = (b - a) / (float)n;
	float local_ans{  };

	for (int i = start; i < end; ++i) {
		float x = a + ((float)i + 0.5) * ban;
		local_ans += functionid(x, intensity);
	}

	jh->sync(local_ans);
	jh->sub();
}


/// Submits the integration job to \p tps using the \p gran as the basis of how to split in to
/// tasks. I had to make this a template because I was using boost::asio::thread_pool to do
/// some bug hunting.
template <typename T>
JobHandle* submit_job(T& tps, func_t functionid, int a, int b, int n, int gran, int intensity) {
	int start = 0;
	int end = gran;

	auto jh = new JobHandle{ (b - a) / (float)n };
	while (start + gran < n) {
		jh->add();
		tps.post([=]() { partial_integrate( functionid, a, b, n, intensity, start, end, jh ); }); // capture by value
		start = end;
		end += gran;
	}

	jh->add();
	tps.post([=]() { partial_integrate( functionid, a, b, n, intensity, start, n, jh ); });

	return jh;
}


// Tuple is functionid, n, intensity, gran, answer. Too lazy to create custom object (bad idea, I know)
/// Recives integration jobs from a file
std::vector<std::tuple<func_t, int, int, int, float>> get_jobs(std::string_view fname) {
	std::ifstream file{ fname.data() };
	std::vector<std::tuple<func_t, int, int, int, float>> ret_val;

	for (std::string line; std::getline(file, line); ) {
		std::stringstream ss{ line };

		int tmp;
		func_t func{ };
		ss >> tmp;
		switch (tmp) {
			case 1: func = f1; break;
			case 2: func = f2; break;
			case 3: func = f3; break;
			case 4: func = f4; break;
			default: {
				std::cout << "[E] Ya, you screwed up... have fun" << std::endl;
				return {  };
			}
		}

		int n, intensity, nbthreads;
		ss >> n;
		ss >> intensity;
		ss >> nbthreads;

		std::string type;
		ss >> type;

		int gran;
		ss >> gran;
		gran = n;

		float ans;
		ss >> ans;

		ret_val.emplace_back(func, n, intensity, gran, ans);
	}

	return ret_val;
}


inline bool fexits(const char* name) {
	std::ifstream f(name);
	return f.good();
}


int main(int argc, char* argv[]) {
	int nbthreads = std::thread::hardware_concurrency();

	if (argc < 2) {
		std::cerr<<"usage: "<<argv[0]<<" <nbthreads>"<<std::endl;
		if (nbthreads == 0) {
			std::cerr << "unable to get number of threads" << std::endl;
			return EXIT_FAILURE;
		} else {
			std::cerr << "using std::thread::hardware_concurrency to get nbthreads" << std::endl;
		}
	} else {
		nbthreads = std::atoi(argv[1]);
	}

	#ifdef CASES_FILE
		const char rloc[] = "../dynamic/cases.txt";
		const char* loc = "/home/aryan/Projects/ITCS3145/A3/dynamic/cases.txt";
		if (fexits(rloc))
			loc = rloc;
		std::cout << "[I] Pulling Jobs from cases.txt..."<< std::endl;
		auto todo = get_jobs(loc);
	#else
		std::cout << "[I] Manually adding jobs..."<< std::endl;
		std::vector<std::tuple<func_t, int, int, int, float>> todo{  };
		// Tuple is functionid, n, intensity, gran, answer
		// todo.emplace_back(f1, 1'000, 1'0000, 100, 50);
		todo.emplace_back(f1, 100'007, 1, 10, 50);
		//todo.emplace_back(f2, 1'000'000, 10, 10'000, 333.333);
		//todo.emplace_back(f2, 1'000'000, 10, 100, 333.333);
		// todo.emplace_back(f3, 1'000'000, 10, 10'000, 1.83908);
		// todo.emplace_back(f4, 1'000'000, 10, 10'000, 12.1567);

	#endif
	std::cout << "[I] Starting ThreadPool threads (" << nbthreads << " threads)" << std::endl;
	ThreadPoolSchedular tps{ nbthreads };

	int a = 0;
	int b = 10;

	std::cout << "[I] Posting jobs for threadpool to do..." << std::endl;
	std::vector<std::tuple<JobHandle*, float>> jobs;
	for (auto [f, n, intensity, gran, answer] : todo) {
		auto jh = submit_job(tps, f, a, b, n, gran, intensity);
		jobs.emplace_back(jh, answer);
		// std::cout << "\r[I] Total Jobs posted: " << jobs.size();
		std::cout.flush();
	}
	// std::cout << std::endl;

	// tps.post([](){ return; }); // dislodge the last node

	// std::cout << "[I] Waiting for jobs to finish..." << std::endl;

	bool done = false;
	while (!done) {
		done = true;
		unsigned count{  };
		for (auto [handle, correct] : jobs) {
			if (!handle->done()) {
				done = false;
				++count;
			}
		}
		// std::cout << "\r[I] Waiting on " << count << " jobs    ";
		std::cout.flush(); // https://stackoverflow.com/questions/14539867/
		std::this_thread::yield();
	}
	// std::cout << std::endl;

	tps.join();

	std::cout << "[I] Jobs finished." << std::endl;

	int nbcorrect = 0;
	for (auto [handle, correct] : jobs) {
		if (std::abs(handle->get() - correct) > 0.1 ) {
			std::cout << "[E] Incorrect: " << handle->get() << " != " << correct << std::endl;
			delete handle;
		} else {
			++nbcorrect;
		}
	}

	std::cout << "[I] Number Correct: " << nbcorrect << std::endl;

	return 0;

}