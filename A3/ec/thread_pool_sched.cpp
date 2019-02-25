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

struct JobHandle {
	float answer = 0;
	std::mutex lock;
	std::atomic_uint16_t left;

	bool done() {
		return left == 0;
	}
};

struct IntegrateWork {
	func_t functionid;
	int a;
	int b;
	int n;
	int intensity;

	int start;
	int end;

	JobHandle* jh;


	void operator() () {
		float ban = (b - a) / (float)n;
		float local_ans{  };
		for (int i = start; i < end; ++i) {
			float x = a + ((float)i + 0.5) * ban;
			local_ans += functionid(x, intensity);
		}

		{
			std::lock_guard { jh->lock };
			jh->answer += local_ans;
		}

		if (jh->left.load() == 1) { // if we are the last thread
			std::lock_guard { jh->lock };
			jh->answer *= (b - a) / (float)n;
		}

		jh->left.fetch_sub(1);
	}
};


JobHandle* submit_job(ThreadPoolSchedular& tps, func_t functionid, int a, int b, int n, int gran, int intensity) {
	int start = 0;
	int end = gran;

	auto jh = new JobHandle{ };
	while (start + gran < n) {
		jh->left.fetch_add(1);
		tps.push(IntegrateWork{ functionid, a, b, n, intensity, start, end, jh });
		start = end;
		end += gran;
	}

	jh->left.fetch_add(1);
	tps.push(IntegrateWork{ functionid, a, b, n, intensity, start, n, jh });

	return jh;
}

// Tuple is functionid, n, intensity, gran, answer
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


int main(int argc, char* argv[]) {
	//const char loc[] = "/home/aryan/Projects/ITCS3145/A3/dynamic/cases.txt";
	const char loc[] = "../dynamic/cases.txt";

	if (argc < 2) {
		std::cerr<<"usage: "<<argv[0]<<" <nbthreads>"<<std::endl;
		return -1;
	}

	int nbthreads = std::atoi(argv[1]);

	std::cout << "[I] Pulling Jobs from cases.txt..."<< std::endl;
	auto todo = get_jobs(loc);

	std::cout << "[I] Starting ThreadPool threads (" << nbthreads << " threads)" << std::endl;
	ThreadPoolSchedular tps{ nbthreads };

	int a = 0;
	int b = 10;

	std::cout << "[I] Posting jobs for threadpool to do..." << std::endl;
	std::vector<std::tuple<JobHandle*, float>> jobs;
	for (auto [f, n, intensity, gran, answer] : todo) {
		auto jh = submit_job(tps, f, a, b, n, gran, intensity);
		jobs.emplace_back(jh, answer);
	}

	std::cout << "[I] Waiting for jobs to finish... Total Jobs posted: " << jobs.size() << std::endl;

	bool done = false;
	while (!done) {
		done = true;
		for (auto [handle, correct] : jobs) {
			if (!handle->done()) {
				done = false;
			}
		}
	}

	tps.end();

	std::cout << "[I] Jobs finished." << std::endl;

	int nbcorrect = 0;
	for (auto [handle, correct] : jobs) {
		if (std::abs(handle->answer - correct) > 0.001 ) {
			std::cout << "[E] Incorrect: " << handle->answer << " != " << correct << std::endl;
		} else {
			++nbcorrect;
		}
	}

	std::cout << "[I] Number Correct: " << nbcorrect << std::endl;

	return 0;

}