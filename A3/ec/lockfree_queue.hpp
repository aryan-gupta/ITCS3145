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

	std::atomic<node_ptr_t> mHead;
	std::atomic<node_ptr_t> mTail;
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
		if (!tail->next.compare_exchange_weak(tailNext, next)) {
			mTail.compare_exchange_strong(tail, tailNext);
			return false;
		}
		mTail.compare_exchange_strong(tail, tailNext);
		return true;
	}

	node_ptr_t pop(node_ptr_t dummy) {
		node_ptr_t head = mHead.load();
		node_ptr_t tail = mTail.load();
		if (head == nullptr) { // empty queue, loop
			return nullptr;
		} else if (tail == head) { // queue has one element
			// try to take ownership of tail node
			node_ptr_t tailNext = tail->next.load();
			if (tailNext == nullptr) { // Check if we can get ownership of tail next pointer
				if (tail->next.compare_exchange_weak(tailNext, dummy)) {
					// we now have ownership of tail node so no other thread can mess with it. update head node to reflect
					if (mHead.compare_exchange_strong(head, nullptr)) {
						if (mTail.compare_exchange_strong(tail, nullptr)) {
							return head;
						}
					}
				}
			}
			return nullptr;
		} else {
			node_ptr_t nextHead = head->next.load();
			if (mHead.compare_exchange_weak(head, nextHead))
				return head;
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

	lockfree_queue() : mHead{ new_node() }, mTail{ mHead.load() }, mAlloc{ } { }


	/// Destroys the queue, pops all the elements out of the queue. Would be a smart idea to
	/// set mHead and mTail to prevent other threads from accessing the data
	~lockfree_queue() {
		while (mHead.load() != nullptr) {
			pop();
		}
	}


	/// Pushed \p element into the queue
	void push(const T& element) {
		node_ptr_t next = new_node(element);
		while (!push(next));
	}


	/// Pops an element off the list and returns it
	T pop() {
		node_ptr_t dummy = new_node(T{ });
		node_ptr_t node = nullptr;
		while (node == nullptr) {
			node = pop(dummy);
		}
		delete_node(dummy);
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
		node_ptr_t dummy = new_node(T{ });
		node_ptr_t node = pop(dummy);
		delete_node(dummy);

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
	std::pair<bool, T> try_pop(O op) {
		node_ptr_t dummy = new_node(T{ });
		node_ptr_t node = nullptr;

		/// keep trying to pop until we are successfull or op returns false
		while (node == nullptr and op()) {
			node = pop(dummy);
		}

		delete_node(dummy);
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