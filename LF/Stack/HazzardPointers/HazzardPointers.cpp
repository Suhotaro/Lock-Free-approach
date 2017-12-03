#include "stdafx.h"
#include <iostream>
#include <atomic>
#include <memory>
#include <thread>

using namespace std;

template <typename T>
class LFStackHP
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

	atomic<Node*> head{ nullptr };
	atomic<Node*> toBeDeleted{ nullptr };
	atomic<unsigned> threadsInPop{ 0 };

	
	struct HazardPointer
	{
		atomic<thread::id> id;
		atomic<void*> pointer;
	};
	enum { MAX_HAZARD_POINTERS = 100, };
	array<HazardPointer, MAX_HAZARD_POINTERS>  hazardPointers;

	class HPOwner {
		HazardPointer *hp;
	public:
		HPOwner();
		HPOwner(HPOwner const&) = delete;
		HPOwner operator=(HPOwner const&) = delete;
		~HPOwner();

		atomic<void*>& getPointer() { return hp->pointer; }
	};

	atomic<void*>& getHazardPointerForCurrentThread();

	void DeleteNodes(Node *nodes);
	void TryReclaim(Node *oldHead);
	void ChainPendingNodes(Node* nodes);
	void ChainPendingNodes(Node* first, Node* last);
	void ChainPendingNode(Node* n);
public:
	LFStackHP() = default;
	~LFStackHP();

	void push(T const& data);
	shared_ptr<T> pop();
	void show();
};

template <typename T>
LFStackHP<T>::HPOwner::HPOwner() : hp(nullptr) {
	for (unsigned i = 0; i < MAX_HAZARD_POINTERS; i++) {
		thread::id oldID;
		if (hazardPointers[i].id.compare_exchange_strong(oldID, this_thread::get_id())) {
			hp = &hazardPointers[i];
			break;
		}
	}
	if (!hp) {
		throw runtime_error("No hazard pointer available");
	}
}

template <typename T>
LFStackHP<T>::HPOwner::~HPOwner() {
	hp->pointer.store(nullptr);
	hp->id.store(thread::id());
}

template <typename T>
LFStackHP<T>::~LFStackHP()
{
	Node *node = head.load();
	while (node)
	{
		Node *next = node->next;
		delete node;
		node = next;
	}
}

template <typename T>
atomic<void*>&  LFStackHP<T>::getHazardPointerForCurrentThread()
{
	thread_local static hpOwner hazard;
	return hazard.getPointer();
}

template <typename T>
void LFStackHP<T>::push(T const& data)
{
	Node *const newNode = new Node(data);
	newNode->next = head.load();
	while (!head.compare_exchange_weak(newNode->next, newNode))
		;
}





template <typename T>
shared_ptr<T> LFStackHP<T>::pop()
{
	++threadsInPop;
	Node* oldHead = head.load();
	while (oldHead &&
		!head.compare_exchange_weak(oldHead,
			oldHead->next))
		;

	shared_ptr<T> res;
	if (oldHead)
		res.swap(oldHead->data);
	TryReclaim(oldHead);
	return res;
}

template <typename T>
void LFStackHP<T>::TryReclaim(Node *oldHead)
{
	if (threadsInPop == 1)
	{
		Node *nodesToDelete = toBeDeleted.exchange(nullptr);
		if (!--threadsInPop)
			DeleteNodes(nodesToDelete);
		else if (nodesToDelete)
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
void LFStackHP<T>::DeleteNodes(Node *nodes)
{
	while (nodes)
	{
		Node *next = nodes->next;
		delete nodes;
		nodes = next;
	}
}

template <typename T>
void LFStackHP<T>::ChainPendingNodes(Node* first, Node* last)
{
	last->next = toBeDeleted;
	while (!toBeDeleted.compare_exchange_weak(last->next,
		first))
		;
}

template <typename T>
void LFStackHP<T>::ChainPendingNodes(Node* nodes)
{
	Node* last = nodes;
	while (Node *const next = last->next)
		last = next;
	ChainPendingNodes(nodes, last);
}

template <typename T>
void LFStackHP<T>::ChainPendingNode(Node* n)
{
	ChainPendingNodes(n, n);
}

template <typename T>
void LFStackHP<T>::show()
{
	Node* n = head.load();
	while (n)
	{
		printf("%d \n", *n->data);
		n = n->next;
	}
}

int main()
{
	//TODO: add multithreding

	


	return 0;
}