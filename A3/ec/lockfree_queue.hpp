/// I just want to take a moment to congratulate myself because this code not only compiled on the second
/// try but it runs without bugs (so far). This seems too good to be true.

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

	// pushes a node into the queue
	// The algo goes: load tail node, if this node is nullptr then we have an empty queue
	// Set the tail node to the new node atomically if the queue is still empty. Here, no
	// Consumer can see this node beacuse the head node is still null. Any producer trying to
	// add another element will see Tail node as the new node, but the head node will still be null.
	// However this wont matter, becase that thread will not be touching head node. We know that
	// if the tail node was nullptr the head node must be too, so update the head node with the new value.
	// We dont need CAS here so I will remove it later. Im just keeping it for sainity checks @todo
	// the new element is now published. If the queue is not empty. then take ownership of the tail
	// node by setting the tail next pointer to next value. If we cant do this then another thread
	// is trying to push a value so we will repeat this loop. If we can get ownership of the next
	// pointer then publish the tail pointer with the new added node. The next pointer of the tail
	// acts as a lock/signal that tells the other threads an element is being pushed.
	// @param next The node to push in to the queue
	// @return If the node was successfully pushed into the queue
	bool push(node_ptr_t next) {
		node_ptr_t tail = mTail.load();
		if (tail == nullptr) { // Then the queue is empty.
			if (mTail.compare_exchange_weak(tail, next)) {
				node_ptr_t head = nullptr;
				if (!mHead.compare_exchange_strong(head, next)) {
					throw 1;
				}
				return true;
			}
			return false;
		} else {
			node_ptr_t tailNext = tail->next.load();
			if (tailNext == nullptr) { // Check if we can get ownership of tail next pointer
				if (tail->next.compare_exchange_weak(tailNext, next)) {
					if (!mTail.compare_exchange_strong(tail, next)) {
						throw 2;
					}
					return true;
				}
				return false;
			} else { // if we cant get ownership then repeat trying to get ownership
				return false;
			}
		}
	}



	// pops a node from the queue
	// The algo goes: Create a dummy node for future use. We may need it, we may not. If the head is null
	// that means we have a empty queue, spin on this loop until we get more work. If the tail equals the
	// head that means we only have one element in the queue, therefore we must take special precautions
	// first we try to take ownership of the tail node by setting the tail's next pointer to a dummy node
	// if we cant get ownership (another thread is updating, etc...) we loop. Once we have ownership, We
	// first change head. This will signal other consumers that the queue is now empty. No producer can
	// update this because of our ownership of the tail node. No other consumer can steal that node because
	// They wont be able to get ownership of tail. Once we are able to set the head node to null, do the same
	// to the tail node. Now that the tail node is null, producers can push more elements into this queue
	// if the queue has alot of elements then just move the head pointer forward and consume one.
	// @param dummy A dummy node, it is passed as a parameter so we keep allocating memory if this is run
	//              in a loop
	// @return The popped node from the queue, null if unsuccessful
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

	lockfree_queue() = default;


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

	/// In place creates the node using the arguments then pushes it into the queue
	template <typename... Args>
	void emplace(Args... args) {
		node_ptr_t next = new_node(std::forward<Args>(args)...);
		while (!push(next));
	}
};

} // end namespace ari