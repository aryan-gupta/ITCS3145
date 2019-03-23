/// An wait-free MTMC queue.
///     - T must be default construtable
///     - If T does not have a nothrow move/copy constructable/assignable
///       semantics, then smart_ptr semantics must be used
///     - All smart_ptr objects must be destroyed BEFORE the queue is destroyed

#include <atomic>
#include <memory>
#include <utility>
#include <new>

namespace ari {

/// Internal impl of a lockfree queue node.
template <typename T>
struct lfq_node {
	using node_ptr_t = lfq_node*; //< Node pointer to next element
	using value_type = T; //< Data type


	/// Default constructor
	lfq_node() = default;


	/// Inplace constructs an node with given parameters
	/// @param n The next pointer
	/// @param args The arguments to construct the data with
	template <typename... Args>
	lfq_node(node_ptr_t n, Args&&... args) : next{ n }, data{ std::forward<Args>(args)... } {  }


	/// Default constructs the data with the next pointer
	/// @param n The next pointer
	lfq_node(node_ptr_t n) : next{ n }, data{  } {  }


	std::atomic<node_ptr_t> next; //< Pointer to the next node
	value_type data; //< The internal data
};


/// An allocator aware lock-free impl of a thread-safe queue.
template <typename T, typename A = std::allocator<T>>
class lockfree_queue {
	using node_t = lfq_node<T>; //< Node type
	using node_ptr_t = lfq_node<T>*; //< Node pointer type
	using node_allocator_type = typename std::allocator_traits<A>::template rebind_alloc<node_t>; //< Node allocator type
	using node_allocator_traits_type = std::allocator_traits<node_allocator_type>; //< Node allocator helper class

	#ifdef __cpp_lib_thread_hardware_interference_size
	static constexpr size_t cache_line = std::hardware_destructive_interference_size;
	#else
	static constexpr size_t cache_line = 64;
	#endif


	alignas(cache_line) struct {
		const node_ptr_t ptr; //< The dummy node
		std::atomic_flag in; //< Flag to notify if queue has dummy node
	} mDummy;
	alignas(cache_line) std::atomic<node_ptr_t> mHead; //< Head node of the queue
	alignas(cache_line) std::atomic<node_ptr_t> mTail; //< Tail node of the queue
	alignas(cache_line) node_allocator_type mAlloc; //< Allocator for node


	// Constructs a new node with the next pointer pointing to null
	// @param args The arguments from which to construct the node from
	// @return The new node
	template <typename... Args>
	node_ptr_t new_node(Args&&... args) {
		auto ptr = node_allocator_traits_type::allocate(mAlloc, 1);
		node_allocator_traits_type::construct(mAlloc, ptr, nullptr, std::forward<Args>(args)...);
		return ptr;
	}


	// Constructs a new node with the next pointer pointing to null. The data is default init
	// @return The new node
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


	/// @todo Update memory barriers for even faster performance
	/// Pushes a node into the queue.
	/// @param next The node to insert into the node
	/// @return bool Whether the node was successfully inserted into the queue
	// The impl goes like this. One invariant is that there is ALWAYS one node in
	// the queue. This allows us to push an element into it without having to worry
	// the consumers and producers fighting for the head node. If the tail node's
	// next pointer is null then we have the ability to insert a node. So we will
	// CAS on that node. This node will never "disapear" because one of the invariants
	// is that there is at least one node in the queue. Once the next node is published
	// consumers can pop the current tail node, but we dont care if this is popped. It
	// will just mean that the tail node points to a node before the head node. Which
	// will be corrected either by us or the next producer thread. If the next pointer
	// is taken by another thread then just move the mTail node forward (unnessary, but)
	// if the thread that added the next node dies then we will never be able to add more
	// nodes anymore.
	bool sync_push(node_ptr_t next) {
		node_ptr_t tail = mTail.load();
		node_ptr_t tailNext = nullptr;
		// we cant have spurious failures here or the mTail node will be set to nullptr
		if (tail->next.compare_exchange_strong(tailNext, next)) {
			mTail.compare_exchange_weak(tail, next);
			return true;
		}
		mTail.compare_exchange_weak(tail, tailNext);
		return false;
	}


	/// Trys to push a dummy node into the queue. Will not always success and does not block
	/// @param func Whether to use unsync_push or sync_push to push the dummy node
	/// @note Is thread-safe is sync_push is used. Is non-blocking
	void push_dummy(bool(lockfree_queue::* func)(node_ptr_t)) {
		if (!mDummy.in.test_and_set()) {
			mDummy.ptr->next = nullptr;
			if(!sync_push(mDummy.ptr))
				mDummy.in.clear();
		}
	}

	// if we get the dummy then we want to clear the status variable and return nullptr
	// because we couldn't pop out a valid node.
	node_ptr_t pop_dummy(node_ptr_t node) {
		if (node == mDummy.ptr) {
			node->next = nullptr;
			mDummy.in.clear();
			return nullptr;
		}
		return node;
	}


	/// Pops a node from the queue.
	/// @return The popped node if successful, or nullptr if we failed
	// This part is more complicated because we need to be use a seperate algo if there
	// is only one node in the queue, to prevent broken invariants. We first test if there
	// is only one node in the queue. If so, we have ONE thread push in the dummy node. This
	// will allow us to pop the last node out of the queue. We use a std::atomic_flag to signal
	// to other threads that we are going to be the one to push the dummy node. There is an issue
	// here where the push can fail. For now Im putting it into a loop, but this can be blocking.
	// I will fix this later. Once we push the dummy node, we failed in popping a node so we return
	// and let another thread pop the non-dummy node. If there is more than one node in the
	// queue, then we just move the head forward. If we popped the dummy node then we clear the
	// flag and return that we failed. If we have a valid node then we return the popped
	// node, if we cant move the head forward then just return we failed.
	// There is a few optimizations we can do here, for example, after we insert the dummy node, might
	// as well try to pop that single node. But let me commit this so I dont lose it.
	node_ptr_t sync_pop() {
		node_ptr_t head = mHead.load();
		node_ptr_t headNext = head->next.load();
		if (headNext == nullptr) { // we have one node then push a dummy node so we can pull out the last node
			push_dummy(&lockfree_queue::sync_push);
			return nullptr;
		} else {
			if (mHead.compare_exchange_weak(head, headNext)) {
				return pop_dummy(head);
			}
			return nullptr;
		}
	}

public:
	// see https://en.cppreference.com/w/cpp/container/queue
	using value_type = T;
	using reference = T&;
	using const_reference = const T&;
	using allocator_type = A;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	/// Constructs queue with no elements
	lockfree_queue()
		: mDummy{ new_node(), ATOMIC_FLAG_INIT }
		, mHead{ mDummy.ptr }
		, mTail{ mDummy.ptr }
		, mAlloc{ }
	{ while(mDummy.in.test_and_set()); }


	/// Destroys the queue, pops all the elements out of the queue. Would be a smart idea to
	/// set mHead and mTail to prevent other threads from accessing the data
	~lockfree_queue() {
		while (mHead.load(std::memory_order_relaxed) != mDummy.ptr) {
			node_ptr_t node = sync_pop();
			if (node != nullptr) delete_node(node);
		}
		delete_node(mDummy.ptr);
	}


	/// Pushed \p element into the queue
	/// @param element The element to push into the queue
	void push(const_reference element) {
		node_ptr_t next = new_node(element);
		while (!sync_push(next));
	}

	/// Pops an element off the list and returns it
	/// @return The popped element
	/// @note thread-safe and blocking
	template <typename U = value_type>
	U pop() {
		node_ptr_t node = nullptr;
		while ((node = sync_pop()) == nullptr);
		U data = node->data;
		delete_node(node);
		return data;
	}


	/// Attempts to push an element in the queue in a single pass.
	/// @param element The element to push into the queue
	/// @note this is thread-safe and non-blocking
	bool try_push(const_reference element) {
		node_ptr_t next = new_node(element);
		bool success = sync_push(next);
		if (!success) delete_node(next);
		return success;
	}

	/// Attempts to pop an element in a single pass
	/// @note this is non-blocking
	/// @return If an element was successfully popped
	/// @return The popped element or a default constructed element
	template <typename U = value_type>
	std::pair<bool, U> try_pop() {
		node_ptr_t node = sync_pop();

		if (node == nullptr) {
			return { false, U{ } };
		} else {
			U data = node->data;
			delete_node(node);
			return { true, data };
		}
	}
};

} // end namespace ari