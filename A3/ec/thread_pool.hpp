
#include <mutex>
#include <queue>
#include <condition_variable>

namespace detail {

/// This class is the base so we can perform some time erasure
struct TPS_func_wrapper_base {
	virtual ~TPS_func_wrapper_base() {  };
	virtual void operator() () = 0;
};


/// This is the derived class of the base
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

/// Class to store and release the memory of a callable
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

} // namespace detail


/// This class is a secdular, you can post functions on the schedular and
/// another thread can pull jobs from this
class ThreadPoolSchedular {
public:
	using callable_t = detail::TPS_callable;

private:
	using func_t = detail::TPS_func_wrapper_base*;
	using lock_t = std::unique_lock<std::mutex>;
	template <typename A> using derived_t = detail::TPS_func_wrapper<A>;

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
		func_t wrapper = new derived_t<T>{ func };
		{
			lock_t lk{ mLock };
			mQ.push(wrapper);
		}
		mSignal.notify_one();
	}

};