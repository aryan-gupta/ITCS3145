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
/// Haha After 3 weeks, I finally got this working. MUAUAUAUA. It is non-blocking, and lock-free.
/// Im happy, the impl is my own (with inspiration from above resources).

#include <atomic>
#include <memory>
#include <utility>
#include <new>

namespace ari {

/// These 4 functions allows use to choose to move or copy the object depeneding on whether it
/// has a nothrow semantics.
// For example if a class has a nothrow move semantics, then it will always move the item out
// of the queue. If there is no nothrow move semantics then it will copy the item out of the queue
template <typename T, typename = std::enable_if_t<
	!std::is_nothrow_move_constructible_v<std::remove_reference_t<T>> and
	std::is_nothrow_copy_constructible_v<std::remove_reference_t<T>>
>>
constexpr auto nothrow_construct(T&& t) -> std::remove_reference_t<T>&
	{ return t; }

template <typename T, typename = std::enable_if_t<std::is_nothrow_move_constructible_v<std::remove_reference_t<T>>>>
constexpr auto nothrow_construct(T&& t) -> std::remove_reference_t<T>&&
	{ return static_cast<std::remove_reference_t<T>&&>(t); }

template <typename T, typename = std::enable_if_t<
	!std::is_nothrow_move_assignable_v<std::remove_reference_t<T>> and
	std::is_nothrow_copy_assignable_v<std::remove_reference_t<T>>
>>
constexpr auto nothrow_assign(T&& t) -> std::remove_reference_t<T>&
	{ return t; }

template <typename T, typename = std::enable_if_t<std::is_nothrow_move_assignable_v<std::remove_reference_t<T>>>>
constexpr auto nothrow_assign(T&& t) -> std::remove_reference_t<T>&&
	{ return static_cast<std::remove_reference_t<T>&&>(t); }


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


/// Impl of a smart_ptr like object that takes care of node clean up if the
/// queue wanted to use *_node_pop() functions. The one thing to note is that
/// ALL node_wrappers must be destroyed BEFORE the orginal queue is destroyed.
/// The wrapper uses the allocator of the original queue to deallocate the nodes
/// Interface is largely based off of std::unique_ptr
template <typename T, typename Q>
class lfq_node_wrapper {
	using node_ptr_t = lfq_node<T>*; //< Node pointer type
	using q_ptr_t = Q*; //< Pointer type to the queue it belongs to (for deallocation)

	node_ptr_t mNode; //< The actual node that this class owns
	q_ptr_t mQ; //< The queue from where this was popped from

public:
	using pointer      = typename std::remove_pointer_t<q_ptr_t>::node_allocator_traits_type::template rebind_alloc<T>::pointer;
	using element_type = typename std::remove_pointer_t<q_ptr_t>::node_allocator_traits_type::template rebind_alloc<T>::value_type;
	using deleter_type = typename std::remove_pointer_t<q_ptr_t>::node_allocator_traits_type::deallocate;

	/// Default constructor
	lfq_node_wrapper() = default;


	/// Constructs a node wrapper with a node and queue
	/// @param node The node to take ownership of
	/// @param q The queue from where \p node originates from
	lfq_node_wrapper(node_ptr_t node, q_ptr_t q) noexcept : mNode{ node }, mQ{ q } {  }


	/// Deleted copy semantics
	lfq_node_wrapper(const lfq_node_wrapper&) = delete; // We cant copy this, because we own the node
	lfq_node_wrapper& operator=(const lfq_node_wrapper&) = delete;


	/// Move constructor, moves ownership from other node to this
	lfq_node_wrapper(lfq_node_wrapper&& other) noexcept : mNode{ other.mNode }, mQ{ other.mQ }
		{ mNode = nullptr; }

	/// Move assignment, moves ownership from other node to this
	lfq_node_wrapper& operator=(lfq_node_wrapper&& other) noexcept {
		release();
		mNode = other.mNode;
		mQ = other.mQ;
		other.mNode = nullptr;
	}


	/// Destructor
	~lfq_node_wrapper() noexcept
		{ release(); }


	/// Releases the internal object if it exists
	void release() noexcept {
		if (mNode)
			mQ->delete_node(mNode);
		mNode = nullptr;
	}


	/// Returns the address to the internal data. nullptr if it owns no data
	/// @return The address to the data, nullptr is it doesnt exist
	pointer get() const noexcept {
		if (mNode == nullptr) return nullptr;
		return &mNode->data;
	}


	/// Converts this object to bool representation.
	/// @return true if this owns a node, false otherwise
	explicit operator bool() const noexcept
		{ return mNode != nullptr; }


	/// Access to the internal data
	/// @return Reference to the internal data
	element_type& operator *() const {
		return mNode->data;
	}

	/// Access to the internal data
	/// @return Pointer to the internal data
	pointer operator ->() const noexcept {
		return get();
	}

};


/// An allocator aware lock-free impl of a thread-safe queue.
template <typename T, typename A = std::allocator<T>>
class lockfree_queue {
	friend class lfq_node_wrapper<T, lockfree_queue>; /// Node wrapper is a friend so we can deallocate nodes

	using node_t = lfq_node<T>; //< Node type
	using node_ptr_t = lfq_node<T>*; //< Node pointer type
	using node_allocator_type = typename std::allocator_traits<A>::template rebind_alloc<node_t>; //< Node allocator type
	using node_allocator_traits_type = std::allocator_traits<node_allocator_type>; //< Node allocator helper class

	#ifdef __cpp_lib_thread_hardware_interference_size
	static constexpr size_t cache_line = std::hardware_destructive_interference_size;
	#else
	static constexpr size_t cache_line = 64;
	#endif


	alignas(cache_line) node_ptr_t mDummy; //< The dummy node
	alignas(cache_line) std::atomic<node_ptr_t> mHead; //< Head node of the queue
	alignas(cache_line) std::atomic<node_ptr_t> mTail; //< Tail node of the queue
	alignas(cache_line) std::atomic_flag mHasDummy; //< Flag to notify if queue has dummy node
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
		if (!tail->next.compare_exchange_strong(tailNext, next)) {
			mTail.compare_exchange_weak(tail, tailNext);
			return false;
		}
		mTail.compare_exchange_weak(tail, next);
		return true;
	}


	/// Trys to push a dummy node into the queue. Will not always success and does not block
	/// @param func Whether to use unsync_push or sync_push to push the dummy node
	/// @note Is thread-safe is sync_push is used. Is non-blocking
	void push_dummy(bool(lockfree_queue::* func)(node_ptr_t)) {
		if (!mHasDummy.test_and_set())
			if(!(this->*func)(mDummy))
				mHasDummy.clear();
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
		if (head->next == nullptr) { // we have one node then push a dummy node so we can pull out the last node
			push_dummy(&lockfree_queue::sync_push);
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



	/// Pushes a node into the queue. Differs from sync_push because it is not thead-safe
	/// but can lead to better performance by not using CAS and using relaxed memory ordering
	/// @param next The new node to push into the queue
	/// @return Always return true
	bool unsync_push(node_ptr_t next) {
		node_ptr_t tail = mTail.load(std::memory_order_relaxed);
		tail->next = next;
		mTail.store(next, std::memory_order_relaxed);
		return true;
	}


	/// Pops an element from the queue. Differs from sync_pop because it is not thead-safe
	/// but can lead to better performance by not using CAS and using relaxed memory ordering
	/// @return The popped node, nullptr if empty queue
	node_ptr_t unsync_pop() {
		node_ptr_t head = mHead.load(std::memory_order_relaxed);

		if (head == mDummy)
			return nullptr;

		if (head->next == nullptr)
			push_dummy(&lockfree_queue::unsync_push);

		mHead.store(head->next, std::memory_order_relaxed);
		return head;
	}


public:
	using value_type = T;
	using reference = T&;
	using const_reference = const T&;
	using allocator_type = A;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using smart_ptr_type = lfq_node_wrapper<T, lockfree_queue>;


	// Constructs queue with no elements
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
		while (mHead.load(std::memory_order_relaxed) != mDummy) {
			node_ptr_t node = unsync_pop();
			if (node != nullptr) delete_node(node);
		}
		delete_node(mDummy);
	}


	/// Pushed \p element into the queue
	/// @param element The element to push into the queue
	void push(const_reference element) {
		node_ptr_t next = new_node(element);
		while (!sync_push(next));
	}


	/// Pushed \p element into the queue
	/// @param element The element to push into the queue
	void push(value_type&& element) {
		node_ptr_t next = new_node( std::forward<value_type>(element) );
		while (!sync_push(next));
	}


	/// Pops an element off the list and returns it
	/// @return The popped element
	/// @note thread-safe and blocking
	value_type pop() {
		node_ptr_t node = nullptr;
		do {
			node = sync_pop();
		} while (node == nullptr);
		value_type data = nothrow_construct(node->data);
		delete_node(node);
		return data;
	}


	/// Pops an element and returns a smart_ptr like object referencing
	/// the popped element
	/// @return A smart_ptr_type to the popped object
	/// @note thread-safe and blocking
	smart_ptr_type node_pop() {
		node_ptr_t node = nullptr;

		do {
			node = sync_pop();
		} while (node == nullptr);

		return smart_ptr_type{ node, this };
	}


	/// Pops an element and copt or moves it into \p ret
	/// If value_type cannot be nothrow moved or nothrow copy, this function is ill formed
	/// @param ret The object in which to store the popped object
	/// @todo make this a template so if the user chooses to use smart_ptr then it wont break compile issues
	void pop(reference ret) {
		node_ptr_t node = nullptr;
		do {
			node = sync_pop();
		} while (node == nullptr);
		ret = nothrow_assign(node->data);
		delete_node(node);
		return ret;
	}


	/// Pushes an item into the queue
	/// @param element The element to push into the queue
	/// @note This is not thread-safe and non-blocking
	void unsyncronized_push(const_reference element) {
		node_ptr_t next = new_node(element);
		unsync_push(next);
	}


	/// Pushes an item into the queue
	/// @param element The element to push into the queue
	/// @note This is not thread-safe and non-blocking
	void unsyncronized_push(value_type&& element) {
		node_ptr_t next = new_node( std::forward<value_type>(element) );
		unsync_push(next);
	}


	/// Pops an item from the queue
	/// @return If we were successful in popping an element
	/// @return The popped element, or a default constructed object
	/// @note This is not thread-safe and non-blocking
	std::pair<bool, value_type> unsyncronized_pop() {
		node_ptr_t node = unsync_pop();

		if (node == nullptr) {
			return { false, value_type{ } };
		} else {
			value_type data = nothrow_construct(node->data);
			delete_node(node);
			return { true, data };
		}
	}


	/// Pops an item from the queue
	/// @return If we were successful in popping an element
	/// @return A smart_ptr_type to the popped element, or nullptr smart_ptr_type
	/// @note This is not thread-safe and non-blocking
	std::pair<bool, smart_ptr_type> unsyncronized_node_pop() {
		node_ptr_t node = unsync_pop();
		return { node != nullptr, { node, this } };
	}


	/// Pops an element and copies or moves it into \p ret
	/// If value_type cannot be nothrow moved or nothrow copy, this function is ill formed
	/// @param ret The object in which to store the popped object
	/// @todo make this a template so if the user chooses to use smart_ptr then it wont break compile issues
	/// @note This is not thread-safe and non-blocking
	bool unsyncronized_pop(reference ret) {
		node_ptr_t node = unsync_pop();

		if (node == nullptr) {
			return false;
		} else {
			ret = nothrow_assign(node->data);
			delete_node(node);
			return true;
		}
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


	/// Attempts to push an element in the queue in a single pass.
	/// @param element The element to push into the queue
	/// @note this is thread-safe and non-blocking
	bool try_push(value_type&& element) {
		node_ptr_t next = new_node( std::forward<value_type>(element) );
		bool success = sync_push(next);
		if (!success) delete_node(next);
		return success;
	}


	/// Attempts to pop an element in a single pass
	/// @note this is non-blocking
	/// @return If an element was successfully popped
	/// @return The popped element or a default constructed elemnt
	std::pair<bool, value_type> try_pop() {
		node_ptr_t node = sync_pop();

		if (node == nullptr) {
			return { false, value_type{ } };
		} else {
			value_type data = nothrow_construct(node->data);
			delete_node(node);
			return { true, data };
		}
	}


	/// Pops an item from the queue
	/// @return If we were successful in popping an element
	/// @return A smart_ptr_type to the popped element, or nullptr smart_ptr_type
	std::pair<bool, smart_ptr_type> try_node_pop() {
		node_ptr_t node = sync_pop();
		return { node != nullptr, { node, this } };
	}


	/// Pops an element and copies or moves it into \p ret
	/// If value_type cannot be nothrow moved or nothrow copied, this function is ill formed
	/// @param ret The object in which to store the popped object
	/// @todo make this a template so if the user chooses to use smart_ptr then it wont break compile issues
	bool try_pop(reference ret) {
		node_ptr_t node = sync_pop();

		if (node == nullptr) {
			return false;
		} else {
			ret = nothrow_assign(node->data);
			delete_node(node);
			return true;
		}
	}


	/// In place creates the node using the arguments then pushes it into the queue
	/// @param args The args to use to emplace the element
	template <typename... Args>
	void emplace(Args&&... args) {
		node_ptr_t next = new_node(std::forward<Args>(args)...);
		while (!sync_push(next));
	}
};

} // end namespace ari