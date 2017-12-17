#include "stdafx.h"

#include <memory>
#include <atomic>

using namespace std;

template <typename T>
class LFQueueOneProduverConsumer
{
private:
	struct Node
	{
		shared_ptr<T> data;
		Node* next;

		Node() : next(nullptr) {}
	};
	atomic<Node*> head;
	atomic<Node*> tail;

	Node* PopHead();

public:
	LFQueueOneProduverConsumer();
	~LFQueueOneProduverConsumer();

	void push(T newValue);
	shared_ptr<T> pop();
};

template <typename T>
LFQueueOneProduverConsumer<T>::LFQueueOneProduverConsumer()
: head(new Node), tail(head.load())
{}

template <typename T>
LFQueueOneProduverConsumer<T>::~LFQueueOneProduverConsumer() {
	while (Node* const oldHead = head.load()) {
		head.store(oldHead->next);
		delete oldHead;
	}
}

template <typename T>
void LFQueueOneProduverConsumer<T>::push(T newValue) {
	shared_ptr<T> newData(make_shared<T>(newValue));
	Node* n = new Node;
	Node *const oldTail = tail.load();
	oldTail->data.swap(newData);
	oldTail->next = n;
	tail.store(n);
}

template <typename T>
shared_ptr<T> LFQueueOneProduverConsumer<T>::pop() {
	Node* oldHead = popHead();
	if (!oldHead)
		return shared_ptr<T>();

	shared_ptr<T> const res(oldHead->data);
	delete oldHead;
	return res;
}

template <typename T>
typename LFQueueOneProduverConsumer<T>::Node* LFQueueOneProduverConsumer<T>::PopHead() {
	Node* const oldHead = head.load();
	if (oldHead == tail.load())
		return nullptr;
	head.store(oldHead->next);
	return oldHead;
};


int main()
{


	return 0;
}