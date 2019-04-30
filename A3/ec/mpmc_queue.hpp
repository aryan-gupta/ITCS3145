
// simple and dirty implementation of a multi producer/multi consumer wait free queue

#include <queue>
#include <mutex>

template <typename T>
class simple_mpmc_queue {
	using lock_guard_t = std::unique_lock<std::mutex>;

	std::queue<T> mQ;
	std::mutex mLock;

public:
	simple_mpmc_queue() = default;
	~simple_mpmc_queue() = default;

	/// @WARNING default and copy constructor must not throw or hell breaks lose
	std::pair<bool, T> try_pop() {
		lock_guard_t lk{ mLock };
		if (mQ.empty()) {
			return { false, T{  } };
		} else {
			T ret = mQ.front();
			mQ.pop();
			return { true, ret };
		}
	}

	void push(T value) {
		lock_guard_t lk{ mLock };
		mQ.push(value);
	}
};