
#include <atomic>
#include <memory>


namespace ari {

template <typename T>
struct lfq_node {
	using node_ptr_t = lfq_node*;
	using value_type = T;

	std::atomic<node_ptr_t> next;
	value_type data;
};

template <typename T>
class lockfree_queue {
	using node_t = lfq_node<T>;
	using node_ptr_t = lfq_node<T>*;

	std::atomic<node_ptr_t> mHead;
	std::atomic<node_ptr_t> mTail;

public:

	/// The algo goes: load tail node, if this node is nullptr then we have an empty queue
	/// Set the tail node to the new node atomically if the queue is still empty. Here, no
	/// Consumer can see this node beacuse the head node is still null. Any producer trying to
	/// add another element will see Tail node @todo account for this. We know that if the tail
	/// node was nullptr the head node must be too, so update the head node with the new value.
	/// the new element is now published. If the queue is not empty. then take ownership of the tail
	/// node by setting the tail next pointer to next value. If we cant do this then another thread
	/// is trying to push a value so we will repeat this loop. If we can get ownership of the next
	/// pointer then publish the tail pointer with the new added node.
	void push(const T& element) {
		node_ptr_t next = new node_t{ nullptr, T };

		while (true) {
			node_ptr_t tail = mTail.load();
			if (tail == nullptr) { // Then the queue is empty.
				mTail.compare_exchange_weak(tail, next);
				node_ptr_t head = nullptr;
				mHead.compare_exchange_weak(head, next);
				break;
			} else {
				node_ptr_t tailNext = tail->next.load();
				if (tailNext == nullptr) { // Check if we can get ownership of tail next pointer
					if (tail->next.compare_exchange_weak(tailNext, next)) {
						mTail.compare_exchange_weak(tail, next);
						break;
					}
				} else { // if we cant get ownership then repeat trying to get ownership
					continue;
				}
			}
		}
	}

	T pop() {
		while (true) {
			node_ptr_t head = mHead.load();
			node_ptr_t tail = mTail.load();
			if (head == nullptr) { // queue is empty, loop
				continue;
			} else {
				if (head == tail) { // only one element left in the queue

				} else {
					node_ptr_t nextHead = head->next.load();
					mHead.compare_exchange_strong(head, nextHead);
				}
			}
		}
	}

};

} // end namespace ari