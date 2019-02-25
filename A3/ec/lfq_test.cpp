

#include <vector>
#include <mutex>
#include <iostream>
#include <thread>
#include <iomanip>


#include "lockfree_queue.hpp"
#include "thread_pool.hpp"

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


static std::mutex out_lock{  };
struct caller {
	int id;

	void operator() () {
		{
			std::lock_guard lk{ out_lock };
			std::cout << std::setfill('0') << std::setw(5);
			std::cout << id << std::endl;
		}
		std::this_thread::sleep_for(std::chrono::seconds{ 1 });
	}
};

static std::atomic_uint32_t gNum{  };
void producer_loop(ThreadPoolSchedular& tps) {
	while (gNum < 10) {
		int num = gNum.fetch_add(1);
		tps.push(caller{ num });
	}

}


int main() {
	ThreadPoolSchedular tps{ 8 };

	std::thread{ producer_loop, std::ref(tps) }.join();

	while (1);
}