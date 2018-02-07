#include "stdafx.h"

#include <memory>
#include <atomic>

using namespace std;

template <typename T>
class LFQueueRCTailHelpingThread
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
		unsigned internalCount : 30;
		unsigned externalCounters : 2;
	};

	struct Node
	{
		atomic<T*> data;
		atomic<NodeCounter> count;
		atomic<CountedNodePtr> next;

		Node()
		{
			NodeCounter newCount;
			newCount.internalCount = 0;
			newCount.externalCounters = 2;
			count.store(newCount);
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
			if (!newCounter.internalCount && !newCounter.externalCounters)
				delete this;
		}
	};

	static void IncreaseExternalCounter(atomic<CountedNodePtr>& counter, CountedNodePtr& old_counter);
	static void FreeExternalCounter(CountedNodePtr& oldNodePtr);

	void SetNewTail(CountedNodePtr &oldTail, CountedNodePtr const &newTail)
	{
		Node * const currentTailPtr = oldTail.ptr;
		while (!tail.compare_exchange_weak(oldTail, newTail) && oldTail.ptr == currentTailPtr)
			;
		if (oldTail.ptr == currentTailPtr)
			FreeExternalCounter(oldTail);
		else
			currentTailPtr->ReleaseRef();
	}

public:
	void push(T newValue);
	unique_ptr<T> pop();
};

template <typename T>
void LFQueueRCTailHelpingThread<T>::IncreaseExternalCounter(atomic<CountedNodePtr>& counter, CountedNodePtr& old_counter)
{
	CountedNodePtr new_counter;
	do
	{
		new_counter = old_counter;
		++new_counter.externalCount;
	} while (!counter.compare_exchange_strong(old_counter, new_counter))
		;
	old_counter.externalCount = new_counter.externalCount;
}

template <typename T>
void LFQueueRCTailHelpingThread<T>::FreeExternalCounter(CountedNodePtr& oldNodePtr)
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
	if (!newCounter.internalCount && !newCounter.externalCounters)
		delete ptr;
}

template <typename T>
void LFQueueRCTailHelpingThread<T>::push(T newValue)
{
	unique_ptr<T> newData(new T(newValue));
	CountedNodePtr newNext;
	newNext.ptr = new Node;
	newNext.externalCount = 1;
	CountedNodePtr oldTail = tail.load();

	for (;;) {
		IncreaseExternalCounter(tail, oldTail);
		T* oldData = nullptr;

		if (oldTail.ptr->data.compare_exchange_strong(oldData, newData.get()))
		{
			CountedNodePtr oldNext = { 0 };
			if (!oldTail.ptr->next.compare_exchange_strong(oldNext, newNext))
			{
				delete newNext.ptr;
				newNext = oldNext;
			}
			SetNewTail(oldTail, newNext);
			newData.release();
			break;
		}
		else
		{
			CountedNodePtr oldNext = { 0 };
			if (oldTail.ptr->next.compare_exchange_strong(oldNext, newNext))
			{
				oldNext = newNext;
				newNext.ptr = new Node;
			}
			SetNewTail(oldTail, oldNext);
		}
	}
}

template <typename T>
std::unique_ptr<T> LFQueueRCTailHelpingThread<T>::pop()
{
	CountedNodePtr oldHead = head.load();
	for (;;) {
		IncreaseExternalCounter(head, oldHead);
		Node* const ptr = oldHead.ptr;
		if (ptr == tail.load().ptr) {
			return unique_ptr<T>();
		}

		CountedNodePtr next = ptr->next.load();
		if (head.compare_exchange_strong(oldHead, next)) {
			T* const res = ptr->data.exchange(nullptr);
			FreeExternalCounter(oldHead);
			return unique_ptr<T>(res);
		}
		ptr->ReleaseRef();
	}
}

int main()
{
	LFQueueRCTailHelpingThread<int> v;

	v.push(10);

	auto t = v.pop();

	return 0;
}