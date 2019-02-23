

#include <vector>
#include <mutex>
#include <iostream>
#include <thread>
#include <iomanip>


#include "lockfree_queue.hpp"

static ari::lockfree_queue<int> gQ{  };
static std::mutex out_lock{  };

void consumer_loop() {
	while (true) {
		int i = gQ.pop();
		std::lock_guard lk{ out_lock };
		std::cout << std::setfill('0') << std::setw(5);
		std::cout << i << std::endl;
	}
}

static std::atomic_uint32_t gNum;

void producer_loop() {
	while (gNum < 10000) {
		int num = gNum.fetch_add(1);
		gQ.push(num);
	}
}


int main() {
	std::vector<std::thread> pool{  };
	int max{  };

	max = 100;
	while (max --> 0) {
		pool.emplace_back(consumer_loop);
	}

	max = 100;
	while (max --> 0) {
		pool.emplace_back(producer_loop);
	}

	for (auto& t : pool) {
		t.join();
	}
}