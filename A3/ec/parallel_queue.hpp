
#include <mutex>
#include <condition_variable>
#include <queue>


template <typename T>
class parallel_queue {
	std::mutex mLock;
	std::condition_variable mSignal;
	std::queue<T> mQ;

public:

	template <typename O>
	std::pair<bool, T> try_pop(O alive) {
		T ret{  };
		{
			std::unique_lock lk { mLock };
			if (mQ.empty()) mSignal.wait(lk, [&](){ return !mQ.empty() or !alive(); });
			if (!alive()) return { false, ret };
			ret = mQ.front();
			mQ.pop();
		}
		return { true, ret };
	}

	std::pair<bool, T> try_pop() {
		T ret{  };
		{
			std::unique_lock lk { mLock };
			if (mQ.empty()) return { false, ret };
			ret = mQ.front();
			mQ.pop();
		}
		return { true, ret };
	}

	void push(T o) {
		{
			std::unique_lock lk{ mLock };
			mQ.push(o);
		}
	}

};