
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

static std::atomic_bool gEnd;

struct TPS_func_wrapper_base {
	virtual ~TPS_func_wrapper_base() {  };
	virtual void operator() () = 0;
};

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

class ThreadPoolSchedular {
public:
	using callable_t = TPS_callable;

private:
	using func_t = TPS_func_wrapper_base*;
	using lock_t = std::unique_lock<std::mutex>;

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
		func_t wrapper = new TPS_func_wrapper<T>{ func };
		{
			lock_t lk{ mLock };
			mQ.push(wrapper);
		}
		mSignal.notify_one();
	}

	int size() {
		lock_t lk{ mLock };
		return mQ.size();
	}
};

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

	std::thread t1{ producer_thread_func, std::ref(tps), std::ref(lock), 0, 50 };
	std::thread t2{ producer_thread_func, std::ref(tps), std::ref(lock), 51, 100 };
	t1.join();
	t2.join();

	std::cout << tps.size() << std::endl;

	for (int i = 0; i < 4; ++i) {
		std::thread th{ consumer_thread_func, std::ref(tps) };
		th.detach();
	}

while(1);

}