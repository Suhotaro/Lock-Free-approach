#include "stdafx.h"

#include <memory>
#include <atomic>

using namespace std;

template <typename T>
class LFQueueRCTail
{
private:
	struct Node;

	struct CountedNodePtr {
		int externalCount;
		Node* ptr;
	};

	atomic<CountedNodePtr> head;
	atomic<CountedNodePtr> tail;

	struct NodeCounter {
		unsigned internalCount:30;
		unsigned externalCounters:2;
	};

	struct Node
	{
		atomic<T*> data;
		atomic<NodeCounter> count;
		CountedNodePtr next;

		Node()
		{
			NodeCounter newCount;
			newCount.internalCount = 0;
			newCount.externalCounters = 2;
			count.store(newCount);

			next.ptr = nullptr;
			next.externalCount = 0;

			data.store(nullptr);
		}

		void ReleaseRef()
		{
			NodeCounter oldCounter = count.load();
			NodeCounter newCounter;
			do
			{
				newCounter = oldCounter;
				--newCounter.internalCount;
			} while (!count.compare_exchange_strong(oldCounter, newCounter))
				;
			if(!newCounter.internalCount && !newCounter.externalCounters)
				delete this;
		}
	};

	static void IncreaseExternalCounter(atomic<CountedNodePtr>& counter, CountedNodePtr& old_counter);
	static void FreeExternalCounter(CountedNodePtr& oldNodePtr);
public:
	LFQueueRCTail();
	~LFQueueRCTail();

	void push(T newValue);
	unique_ptr<T> pop();
};

template <typename T>
LFQueueRCTail<T>::LFQueueRCTail()
{
	CountedNodePtr dummy;
	dummy.externalCount = 1;
	dummy.ptr = new Node;
	
	head.store(dummy);
	tail.store(head.load());
}

template <typename T>
LFQueueRCTail<T>::~LFQueueRCTail()
{
	CountedNodePtr current = head.load();
	while (current.ptr) {
		T* data = current.ptr->data.load();
		if (data)
			delete data;

		current = current.ptr->next;
	}
}

template <typename T>
void LFQueueRCTail<T>::IncreaseExternalCounter(
	atomic<CountedNodePtr>& counter, CountedNodePtr& old_counter)
{
	CountedNodePtr new_counter;
	do
	{
		new_counter = old_counter;
		++new_counter.externalCount;
	}
	while(!counter.compare_exchange_strong(old_counter, new_counter))
		;
	old_counter.externalCount = new_counter.externalCount;
}

template <typename T>
void LFQueueRCTail<T>::FreeExternalCounter(CountedNodePtr& oldNodePtr)
{
	Node* const ptr = oldNodePtr.ptr;
	int const countIncrease = oldNodePtr.externalCount - 2;
	NodeCounter oldCounter = ptr->count.load();
	NodeCounter newCounter;
	do
	{
		newCounter = oldCounter;
		--newCounter.externalCounters;
		newCounter.internalCount += countIncrease;
	} while (!ptr->count.compare_exchange_strong(oldCounter, newCounter))
		;
	if(!newCounter.internalCount && !newCounter.externalCounters)
		delete ptr;
}

template <typename T>
void LFQueueRCTail<T>::push(T newValue)
{
	unique_ptr<T> newData(new T(newValue));
	CountedNodePtr newNext;
	newNext.ptr = new Node;
	newNext.externalCount = 1;
	CountedNodePtr oldTail = tail.load();
	for(;;) {
		IncreaseExternalCounter(tail, oldTail);
		T* oldData = nullptr;
		if(oldTail.ptr->data.compare_exchange_strong(oldData, newData.get()))
		{
			oldTail.ptr->next = newNext;
			oldTail = tail.exchange(newNext);
			FreeExternalCounter(oldTail);
			newData.release();
			break;
		}
		oldTail.ptr->ReleaseRef();
	}
}

template <typename T>
std::unique_ptr<T> LFQueueRCTail<T>::pop()
{
	CountedNodePtr oldHead = head.load();
	for (;;) {
		IncreaseExternalCounter(head, oldHead);
		Node* const ptr = oldHead.ptr;
		if(ptr == tail.load().ptr) {
			ptr->ReleaseRef();
			return unique_ptr<T>();
		}
		if(head.compare_exchange_strong(oldHead, ptr->next)) {
			T* const res = ptr->data.exchange(nullptr);
			FreeExternalCounter(oldHead);
			return unique_ptr<T>(res);
		}
		ptr->ReleaseRef();
	}
}

int main()
{
	LFQueueRCTail<int> v;

	v.push(10);
	auto tmp = v.pop();

	return 0;
}