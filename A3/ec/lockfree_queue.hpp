/// I just want to take a moment to congratulate myself because this code not only compiled on the second
/// try but it runs without bugs (so far). This seems too good to be true.
// (first try gave me this error)
//    lockfree_queue.hpp: In member function ‘void ari::lockfree_queue<T>::push(const T&)’:
//    lockfree_queue.hpp:40:44: error: expected primary-expression before ‘}’ token
//         node_ptr_t next = new node_t{ nullptr, T };

#include <atomic>
#include <memory>
#include <utility>

namespace ari {

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

template <typename T, typename A = std::allocator<T>>
class lockfree_queue {
	using node_t = lfq_node<T>;
	using node_ptr_t = lfq_node<T>*;
	using node_allocator_type = typename std::allocator_traits<A>::template rebind_alloc<node_t>;
	using node_allocator_traits_type = std::allocator_traits<node_allocator_type>;

	std::atomic<node_ptr_t> mHead;
	std::atomic<node_ptr_t> mTail;
	node_allocator_type mAlloc;

	template <typename... Args>
	node_ptr_t new_node(Args&&... args) {
		auto ptr = node_allocator_traits_type::allocate(mAlloc, 1);
		node_allocator_traits_type::construct(mAlloc, ptr, nullptr, std::forward<Args>(args)...);
		return ptr;
	}

	void delete_node(node_ptr_t ptr) {
		node_allocator_traits_type::destroy(mAlloc, ptr);
		node_allocator_traits_type::deallocate(mAlloc, ptr, 1);
	}

public:
	using value_type = T;
	using reference = T&;
	using const_reference = const T&;
	using allocator_type = A;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	lockfree_queue() = default;

	~lockfree_queue() {
		while (mHead.load() != nullptr) {
			pop();
		}
	}

	/// The algo goes: load tail node, if this node is nullptr then we have an empty queue
	/// Set the tail node to the new node atomically if the queue is still empty. Here, no
	/// Consumer can see this node beacuse the head node is still null. Any producer trying to
	/// add another element will see Tail node as the new node, but the head node will still be null.
	/// However this wont matter, becase that thread will not be touching head node. We know that
	/// if the tail node was nullptr the head node must be too, so update the head node with the new value.
	/// We dont need CAS here so I will remove it later. Im just keeping it for sainity checks @todo
	/// the new element is now published. If the queue is not empty. then take ownership of the tail
	/// node by setting the tail next pointer to next value. If we cant do this then another thread
	/// is trying to push a value so we will repeat this loop. If we can get ownership of the next
	/// pointer then publish the tail pointer with the new added node. The next pointer of the tail
	/// acts as a lock/signal that tells the other threads an element is being pushed.
	void push(const T& element) {
		node_ptr_t next = new_node(element);

		while (true) {
			node_ptr_t tail = mTail.load();
			if (tail == nullptr) { // Then the queue is empty.
				if (mTail.compare_exchange_weak(tail, next)) {
					node_ptr_t head = nullptr;
					if (!mHead.compare_exchange_strong(head, next)) {
						throw 1;
					}
					break;
				}
			} else {
				node_ptr_t tailNext = tail->next.load();
				if (tailNext == nullptr) { // Check if we can get ownership of tail next pointer
					if (tail->next.compare_exchange_weak(tailNext, next)) {
						if (!mTail.compare_exchange_strong(tail, next)) {
							throw 2;
						}
						break;
					}
				} else { // if we cant get ownership then repeat trying to get ownership
					continue;
				}
			}
		}
	}


	/// The algo goes: Create a dummy node for future use. We may need it, we may not. If the head is null
	/// that means we have a empty queue, spin on this loop until we get more work. If the tail equals the
	/// head that means we only have one element in the queue, therefore we must take special precautions
	/// first we try to take ownership of the tail node by setting the tail's next pointer to a dummy node
	/// if we cant get ownership (another thread is updating, etc...) we loop. Once we have ownership, We
	/// first change head. This will signal other consumers that the queue is now empty. No producer can
	/// update this because of our ownership of the tail node. No other consumer can steal that node because
	/// They wont be able to get ownership of tail. Once we are able to set the head node to null, do the same
	/// to the tail node. Now that the tail node is null, producers can push more elements into this queue
	/// if the queue has alot of elements then just move the head pointer forward and consume one.
	T pop() {
		node_ptr_t dummy = new_node(T{ });

		while (true) {
			node_ptr_t head = mHead.load();
			node_ptr_t tail = mTail.load();
			if (head == nullptr) { // empty queue, loop
				continue;
			} else if (tail == head) { // queue has one element
				// try to take ownership of tail node
				node_ptr_t tailNext = tail->next.load();
				if (tailNext == nullptr) { // Check if we can get ownership of tail next pointer
					if (tail->next.compare_exchange_weak(tailNext, dummy)) {
						// we now have ownership of tail node so no other thread can mess with it. update head node to reflect
						if (mHead.compare_exchange_strong(head, nullptr)) {
							if (mTail.compare_exchange_strong(tail, nullptr)) {
								T data = std::move(head->data);
								delete_node(head);
								delete_node(dummy);
								return data;
							}
						}
					}
				}
			} else {
				node_ptr_t nextHead = head->next.load();
				if (mHead.compare_exchange_weak(head, nextHead)) {
					T data = std::move(head->data);
					delete_node(head);
					delete_node(dummy);
					return data;
				}
			}
		}
		delete_node(dummy);
	}

};

} // end namespace ari