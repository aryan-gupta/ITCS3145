
#include <iostream>
#include <chrono>
#include <cmath>
#include <chrono>
#include <utility>
#include <tuple>
#include <vector>
#include <memory>
#include <stdexcept>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <thread>
#include <functional>

#include "thread_pool.hpp"

static std::atomic_bool gEnd;


/// A callable object to test the ThreadPool Schedular
class ConsolePrinter {
	int mID;
	std::mutex& mCOLock;

public:
	ConsolePrinter() = default;
	ConsolePrinter(int id, std::mutex& lk) : mID{ id }, mCOLock{ lk } {  }

	void operator()() {
		{
			std::unique_lock<std::mutex> lk{ mCOLock };
			std::cout << mID << std::endl;
		}
		std::this_thread::sleep_for(std::chrono::seconds{ 1 });
	}
};


void consumer_thread_func(ThreadPoolSchedular& tps) {
	while (!gEnd) {
		auto func = tps.pop();
		func();
	}
}


std::mutex gLock;
void print_crap(int id) {
	{
		std::unique_lock<std::mutex> lk{ gLock };
		std::cout << id << std::endl;
	}

	std::this_thread::sleep_for(std::chrono::seconds{ 1 });
}


void producer_thread_func(ThreadPoolSchedular& tps, std::mutex& lock, int start, int end) {
	while(start++ != end) {
		// ConsolePrinter printer{ start, std::ref(lock) };
		auto func = std::bind(print_crap, start);
		tps.push(func);
	}
}


int main(int argc, char* argv[]) {
	std::mutex lock{  };
	gEnd = false;

	ThreadPoolSchedular tps{  };

	for (int i = 0; i < 4; ++i) {
		std::thread th{ consumer_thread_func, std::ref(tps) };
		th.detach();
	}

	std::thread t1{ producer_thread_func, std::ref(tps), std::ref(lock), 0, 50 };
	std::thread t2{ producer_thread_func, std::ref(tps), std::ref(lock), 51, 100 };
	t1.join();
	t2.join();


while(1);

}