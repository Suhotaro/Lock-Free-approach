#include "stdafx.h"
#include <iostream>
#include <atomic>
#include <memory>

using namespace std;

template <typename T>
class LockFreeStack
{
private:
	struct Node
	{
		shared_ptr<T> data;
		Node *next;
		Node(T const& data_) :
			data(make_shared<T>(data_)), next(nullptr)
		{}
	};

	atomic<Node*> head{nullptr};
	atomic<Node*> toBeDeleted{nullptr};
	atomic<unsigned> threadsInPop{0};

	void DeleteNodes(Node *nodes);
	void TryReclaim(Node *oldHead);
	void ChainPendingNodes(Node* nodes);
	void ChainPendingNodes(Node* first, Node* last);
	void ChainPendingNode(Node* n);
public:
	LockFreeStack() = default;
	~LockFreeStack();

	void push(T const& data);
	shared_ptr<T> pop();
	void show();
};

template <typename T>
LockFreeStack<T>::~LockFreeStack()
{
	Node *node = head.load();
	while(node)
	{
		Node *next = node->next;
		delete node;
		node = next;
	}
}

template <typename T>
void LockFreeStack<T>::push(T const& data)
{
	Node *const newNode = new Node(data);
	newNode->next = head.load();
	while(!head.compare_exchange_weak(newNode->next, newNode))
		;
}

template <typename T>
shared_ptr<T> LockFreeStack<T>::pop()
{
	++threadsInPop;
	Node* oldHead = head.load();
	while(oldHead &&
		!head.compare_exchange_weak(oldHead,
									oldHead->next))
		;

	shared_ptr<T> res;
	if(oldHead)
		res.swap(oldHead->data);
	TryReclaim(oldHead);
	return res;
}

template <typename T>
void LockFreeStack<T>::TryReclaim(Node *oldHead)
{
	if(threadsInPop == 1)
	{
		Node *nodesToDelete = toBeDeleted.exchange(nullptr);
		if(!--threadsInPop)
			DeleteNodes(nodesToDelete);
		else if(nodesToDelete)
			ChainPendingNodes(nodesToDelete);
		delete oldHead;
	}
	else
	{
		ChainPendingNode(oldHead);
		--threadsInPop;
	}
}

template <typename T>
void LockFreeStack<T>::DeleteNodes(Node *nodes)
{
	while(nodes)
	{
		Node *next = nodes->next;
		delete nodes;
		nodes = next;
	}
}

template <typename T>
void LockFreeStack<T>::ChainPendingNodes(Node* first, Node* last)
{
	last->next = toBeDeleted;
	while(!toBeDeleted.compare_exchange_weak(last->next,
											 first))
		;
}

template <typename T>
void LockFreeStack<T>::ChainPendingNodes(Node* nodes)
{
	Node* last = nodes;
	while(Node *const next = last->next)
		last = next;
	ChainPendingNodes(nodes, last);
}

template <typename T>
void LockFreeStack<T>::ChainPendingNode(Node* n)
{
	ChainPendingNodes(n, n);
}

template <typename T>
void LockFreeStack<T>::show()
{
	Node* n = head.load();
	while(n)
	{
		printf("%d \n", *n->data);
		n = n->next;
	}
}

int main()
{
	//TODO: add multithreding

	LockFreeStack<int> lfstak;

	lfstak.push(5);
	lfstak.push(8);
	lfstak.push(10);

	cout << "show\n";
	lfstak.show();
	cout << endl;

	cout << "pop: " << *lfstak.pop() << endl;
	cout << "pop: " << *lfstak.pop() << endl;
	cout << endl;

	cout << "show2\n";
	lfstak.show();
	cout << endl;

	lfstak.push(1);
	lfstak.push(2);

	cout << "show2\n";
	lfstak.show();
	cout << endl;

	return 0;
}