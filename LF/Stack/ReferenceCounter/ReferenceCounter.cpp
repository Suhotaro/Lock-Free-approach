#include "stdafx.h"
#include <iostream>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>

using namespace std;

template <typename T>
class LFStackRC
{
private:
	struct Node;

	struct CountedNodePtr {
		int externalCount;
		Node* ptr = nullptr;
	};

	struct Node {
		shared_ptr<T> data;
		atomic<int> internalCount;
		CountedNodePtr next;

		Node(T const& data_) : data(make_shared<T>(data_)), internalCount(0) {}
	};

	atomic<CountedNodePtr> head;

	void IncreaseHeadCount(CountedNodePtr& oldCounter);
public:
	~LFStackRC();

	void push(T const& data);
	shared_ptr<T> pop();
	void show();
};

template <typename T>
LFStackRC<T>::~LFStackRC()
{
	while (pop())
		;
}

template <typename T>
void LFStackRC<T>::push(T const& data)
{
	CountedNodePtr newNode;
	newNode.ptr = new Node(data);
	newNode.externalCount = 1;
	newNode.ptr->next = head.load();
	while(!head.compare_exchange_weak(newNode.ptr->next, newNode))
		;
}

template <typename T>
void LFStackRC<T>::IncreaseHeadCount(CountedNodePtr& oldCounter)
{
	CountedNodePtr newCounter;
	do {
		newCounter = oldCounter;
		++newCounter.externalCount;
	} 
	while (!head.compare_exchange_strong(oldCounter, newCounter))
		;

	oldCounter.externalCount = newCounter.externalCount;
}

template <typename T>
shared_ptr<T> LFStackRC<T>::pop()
{
	CountedNodePtr oldHead = head.load();

	for(;;) {
		IncreaseHeadCount(oldHead);
		Node *const ptr = oldHead.ptr;
		if (!ptr)
			return shared_ptr<T>();

		if(head.compare_exchange_strong(oldHead, ptr->next)) {
			shared_ptr<T> res;
			res.swap(ptr->data);

			int const countIncrease = oldHead.externalCount - 2;
			if (ptr->internalCount.fetch_add(countIncrease) == -countIncrease)
				delete ptr;

			return res;
		}
		else if (ptr->internalCount.fetch_sub(1) == 1)
			delete ptr;
	}
}

template <typename T>
void LFStackRC<T>::show()
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
	LFStackRC<int> lfstak;

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
