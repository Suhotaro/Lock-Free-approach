#include "stdafx.h"
#include <iostream>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>

using namespace std;

template <typename T>
class LFStackRCB
{
private:
	struct Node;

	struct CountedNodePtr
	{
		int externalCount;
		Node* ptr = nullptr;
	};

	struct Node
	{
		shared_ptr<T> data;
		atomic<int> internalCount;
		CountedNodePtr next;

		Node(T const& data_) : data(make_shared<T>(data_)), internalCount(0)
		{}
	};

	atomic<CountedNodePtr> head;

	void IncreaseHeadCount(CountedNodePtr& oldCounter);
public:
	~LFStackRCB();

	void push(T const& data);
	shared_ptr<T> pop();
	void show();
};

template <typename T>
LFStackRCB<T>::~LFStackRCB()
{
	while(pop())
		;
}

template <typename T>
void LFStackRCB<T>::push(T const& data)
{
	CountedNodePtr newNode;
	newNode.ptr = new Node(data);
	newNode.externalCount = 1;
	newNode.ptr->next = head.load(memory_order::memory_order_relaxed);
	while(!head.compare_exchange_weak(newNode.ptr->next, newNode,
									  memory_order::memory_order_release,
									  memory_order::memory_order_relaxed))
		;
}

template <typename T>
void LFStackRCB<T>::IncreaseHeadCount(CountedNodePtr& oldCounter)
{
	CountedNodePtr newCounter;
	do
	{
		newCounter = oldCounter;
		++newCounter.externalCount;
	}
	while(!head.compare_exchange_strong(oldCounter, newCounter,
										memory_order::memory_order_acquire,
										memory_order::memory_order_relaxed))
		;

	oldCounter.externalCount = newCounter.externalCount;
}

template <typename T>
shared_ptr<T> LFStackRCB<T>::pop()
{
	CountedNodePtr oldHead = head.load(memory_order::memory_order_relaxed);

	for(;;)
	{
		IncreaseHeadCount(oldHead);
		Node *const ptr = oldHead.ptr;
		if(!ptr)
			return shared_ptr<T>();

		if(head.compare_exchange_strong(oldHead, ptr->next,
										memory_order::memory_order_relaxed))
		{
			shared_ptr<T> res;
			res.swap(ptr->data);

			int const countIncrease = oldHead.externalCount - 2;
			if(ptr->internalCount.fetch_add(countIncrease,
											memory_order::memory_order_release) == -countIncrease)
				delete ptr;

			return res;
		}
		else if(ptr->internalCount.fetch_add(-1, memory_order::memory_order_relaxed) == 1)
		{
			ptr->internalCount.load(memory_order::memory_order_acquire);
			delete ptr;
		}
	}
}

template <typename T>
void LFStackRCB<T>::show()
{
	CountedNodePtr h = head.load();
	Node* n = h.ptr;
	while(n)
	{
		printf("%d \n", *n->data);
		n = n->next.ptr;
	}
}

int main()
{
	LFStackRCB<int> lfstak;

	lfstak.push(5);
	lfstak.push(8);
	lfstak.push(10);

	cout << "show\n";
	lfstak.show();
	cout << endl;

	cout << "pop: " << *lfstak.pop() << endl;
	cout << "pop: " << *lfstak.pop() << endl;
	cout << endl;

	cout << "show\n";
	lfstak.show();
	cout << endl;

	lfstak.push(1);
	lfstak.push(2);

	cout << "show\n";
	lfstak.show();
	cout << endl;

	return 0;
}
