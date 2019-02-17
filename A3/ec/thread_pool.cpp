
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

template <typename T>
struct TPS_node {
	std::atomic<TPS_node*> next;
	T data;
};

class ThreadPoolSchedular {
	using func_t = void (*) ();
	using node_t = TPS_node<func_t>;

	std::atomic<node_t*> mHead;
	std::atomic<node_t*> mTail;
	std::atomic<bool> mEFlag;

public:
	ThreadPoolSchedular() : mHead{  }, mTail{  } {	}

	void pop() {
		auto node = mHead.load(std::memory_order_relaxed);
		do {
			// if the queue is empty, load new value and spin
			if (node == nullptr) {
				node = mHead.load(std::memory_order_relaxed);
				continue;
			}
		} while(mHead.compare_exchange_weak(node, node->next));
	}

	void push(func_t func) {
		/// Ok so heres the algo: if mTail is null then the queue is empty. Then get exclusive access to both mHead and mTail
		/// and add the node. If the queue is not empty
		bool added = false;
		node_t* newNode = new node_t{ nullptr, func };
		// IF the que is empty then we need to update both mHead and mTail. We need the double checked singleton pattern
		/// First check if the node is null. If it is, try to get exclusive access to both
		/// mHead and mTail. Load mTail again. This load can be relaxed because we of the acquire
		/// barrier from the flag. Test for null again. (make sure no other thread updated and
		/// cleared the flag). And then add the node
		/////// AHHHHHHH IM GETTING A BRAIN ANYERISMM
		do {
			auto node = mTail.load(std::memory_order_relaxed);
			if (node == nullptr) {
				bool flag = false;
				// This CES MUST be aquire because if we successfully get exclusive access then we need to see the write to mTail
				// from other threads (to double check that mTail is nullptr). If the flag value is true then do nothing because
				// another thread is updating the value
				if (mEFlag.compare_exchange_strong(flag, true, std::memory_order_acquire, std::memory_order_relaxed)) {
					node = mTail.load(std::memory_order_relaxed);
					if (node == nullptr) {
						mHead = mTail = newNode;
						added = true;
					}
					// We want to see the write to mHead and mTail from this thread in the other theads
					mEFlag.store(false, std::memory_order_release);
				}
			} else {
				node_t* nextNode = nullptr;
				if (node->next.compare_exchange_weak(nextNode, newNode))

				if (mTail.compare_exchange_weak(node, newNode))
					added = true;
			}
		} while (!added);
	}

};

int main(int argc, char* argv[]) {

}