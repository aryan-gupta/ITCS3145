/// I just want to take a moment to congratulate myself because this code not only compiled on the second
/// try but it runs without bugs (so far). This seems too good to be true.
/// ^ LIES, LIES I TELL YOU. the assignment is past due, but I still want this to work.
/// I decided that I am too dumb to figure this out myself. To the interwebs...
/// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.53.8674&rep=rep1&type=pdf
/// ^ this impl doesnt really work very well. The issue with this is that it implies that
/// when we pop an item, we leave a copy of it in the head, that means we dont have 'ownership'
/// of the node. The responsibility of cleanup is left to the next thread that pops, and I dont
/// like it. I will still try to use this and see if I can make is efficient
/// After much research, I was unable to find a practicle impl. Im probs going to have to
/// use this and modify it a bit to suit my tastes.
/// http://blog.shealevy.com/2015/04/23/use-after-free-bug-in-maged-m-michael-and-michael-l-scotts-non-blocking-concurrent-queue-algorithm/
/// https://stackoverflow.com/questions/40818465/explain-michael-scott-lock-free-queue-alorigthm


#include <atomic>
#include <memory>
#include <utility>

namespace ari {

/// Internal impl of a node of the lockfree queue.
template <typename T>
struct lfq_node {
	using node_ptr_t = lfq_node*;
	using value_type = T;

	lfq_node() = default;

	template <typename... Args>
	lfq_node(node_ptr_t n, Args&&... args) : next{ n }, data{ std::forward<Args>(args)... } {  }

	lfq_node(node_ptr_t n) : next{ n }, data{  } {  }

	std::atomic<node_ptr_t> next;
	value_type data;
};


/// An allocator aware lock-free impl of a thread-safe queue.
template <typename T, typename A = std::allocator<T>>
class lockfree_queue {
	using node_t = lfq_node<T>;
	using node_ptr_t = lfq_node<T>*;
	using node_allocator_type = typename std::allocator_traits<A>::template rebind_alloc<node_t>;
	using node_allocator_traits_type = std::allocator_traits<node_allocator_type>;

	node_ptr_t mDummy;
	std::atomic<node_ptr_t> mHead;
	std::atomic<node_ptr_t> mTail;
	std::atomic_flag mHasDummy;
	node_allocator_type mAlloc;


	// Constructs a new node with the next pointer pointing to null
	// @param args The arguments from which to construct the node from
	// @return The new node
	template <typename... Args>
	node_ptr_t new_node(Args&&... args) {
		auto ptr = node_allocator_traits_type::allocate(mAlloc, 1);
		node_allocator_traits_type::construct(mAlloc, ptr, nullptr, std::forward<Args>(args)...);
		return ptr;
	}


	node_ptr_t new_node() {
		auto ptr = node_allocator_traits_type::allocate(mAlloc, 1);
		node_allocator_traits_type::construct(mAlloc, ptr, nullptr);
		return ptr;
	}

	// Deletes a node and the internal data
	// @param ptr The node to delete/deallocate
	void delete_node(node_ptr_t ptr) {
		node_allocator_traits_type::destroy(mAlloc, ptr);
		node_allocator_traits_type::deallocate(mAlloc, ptr, 1);
	}

/// @todo There is alot of extra-ness in this code, we use CAS even though we can
/// just use store. Fix this
/// @todo Update memory barriers for even faster performance

	bool push(node_ptr_t next) {
		node_ptr_t tail = mTail.load();
		node_ptr_t tailNext = nullptr;
		// we cant have spurious failures here or the mTail node will be set to nullptr
		if (!tail->next.compare_exchange_strong(tailNext, next)) {
			mTail.compare_exchange_weak(tail, tailNext);
			return false;
		}
		mTail.compare_exchange_weak(tail, next);
		return true;
	}

	node_ptr_t pop() {
		node_ptr_t head = mHead.load();
		if (head->next == nullptr) { // we have one node then push a dummy node so we can pull out the last node
			if (!mHasDummy.test_and_set()) // only one thread gets to push the dummy
				while(!push(mDummy));
			return nullptr;
		} else {
			if (mHead.compare_exchange_weak(head, head->next)) {
				// if we get the dummy then we want to clear the status variable and return nullptr
				// because we couldn't pop out a valid node.
				if (head == mDummy) {
					mDummy->next = nullptr;
					mHasDummy.clear();
					return nullptr;
				} else {
					return head;
				}
			}
			return nullptr;
		}
	}

public:
	using value_type = T;
	using reference = T&;
	using const_reference = const T&;
	using allocator_type = A;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	lockfree_queue()
		: mDummy{ new_node() }
		, mHead{ mDummy }
		, mTail{ mDummy }
		, mHasDummy{ ATOMIC_FLAG_INIT }
		, mAlloc{ }
	{ while(mHasDummy.test_and_set()); }


	/// Destroys the queue, pops all the elements out of the queue. Would be a smart idea to
	/// set mHead and mTail to prevent other threads from accessing the data
	~lockfree_queue() {
		while (mHead.load() != mDummy) {
			node_ptr_t node = pop();
			if (node != nullptr) delete_node(node);
		}
		delete_node(mDummy);
	}


	/// Pushed \p element into the queue
	void push(const T& element) {
		node_ptr_t next = new_node(element);
		while (!push(next));
	}


	/// Pops an element off the list and returns it
	T wait_pop() {
		node_ptr_t node = nullptr;
		do {
			node = pop();
		} while (node == nullptr);
		T data = std::move(node->data);
		delete_node(node);
		return data;
	}


	/// Attempts to push an element in the queue in a single pass.
	/// @note this is non-blocking
	bool try_push(const T& element) {
		node_ptr_t next = new_node(element);
		bool success = push(next);
		if (!success) delete_node(next);
		return success;
	}


	/// Attempts to pop an element in a single pass
	/// @note this is non-blocking
	std::pair<bool, T> try_pop() {
		node_ptr_t node = pop();

		if (node == nullptr) {
			return { false, T{ } };
		} else {
			T data = std::move(node->data);
			delete_node(node);
			return { true, data };
		}
	}

	/// Trys to pop until the op returns false
	template <typename O>
	[[deprecated]]
	std::pair<bool, T> try_pop(O op) {
		node_ptr_t node = nullptr;

		/// keep trying to pop until we are successfull or op returns false
		while (node == nullptr and op()) {
			node = pop();
		}

		if (node == nullptr) {
			return { false, T{ } };
		} else {
			T data = std::move(node->data);
			delete_node(node);
			return { true, data };
		}
	}


	/// In place creates the node using the arguments then pushes it into the queue
	template <typename... Args>
	void emplace(Args&&... args) {
		node_ptr_t next = new_node(std::forward<Args>(args)...);
		while (!push(next));
	}
};

} // end namespace ari