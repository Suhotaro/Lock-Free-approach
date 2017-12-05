#include "stdafx.h"
#include <iostream>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>

using namespace std;

struct HazardPointer
{
	atomic<thread::id> id;
	atomic<void*> pointer;
};

unsigned const MAX_HAZARD_POINTERS = 100;
HazardPointer hazard_pointers[MAX_HAZARD_POINTERS];

class HPOwner
{
	HazardPointer *hp;
public:
	HPOwner();

	HPOwner(HPOwner const&) = delete;
	HPOwner operator=(HPOwner const&) = delete;

	~HPOwner();

	atomic<void *>& GetPointer();
};

HPOwner::HPOwner()
: hp(nullptr)
{
	for(unsigned i = 0; i < MAX_HAZARD_POINTERS; i++)
	{
		thread::id old_id;
		if(hazard_pointers[i].id.compare_exchange_weak(
			old_id, this_thread::get_id()))
		{
			hp = &hazard_pointers[i];
			break;
		}

		if(!hp)
			throw runtime_error("No hazard pointers available");
	}
}

HPOwner::~HPOwner()
{
	hp->pointer.store(nullptr);
	hp->id.store(thread::id());
}

atomic<void *>& HPOwner::GetPointer()
{
	return hp->pointer;
}

atomic<void*> & GetHazardPointerForCurrentThread()
{
	thread_local static HPOwner hazard;
	return hazard.GetPointer();
}

bool HasHazardPointerFor(void *p)
{
	for(unsigned i = 0; i < MAX_HAZARD_POINTERS; i++)
		if(hazard_pointers[i].pointer.load() == p)
			return true;
	return false;
}

template <typename T>
void DoDelete(void *p)
{
	delete static_cast<T*>(p);
}

struct DataToReclaim {
	void *data;
	function<void(void*)> deleter;
	DataToReclaim *next;

	template <typename T>
	DataToReclaim(T* p);
	~DataToReclaim();
};

atomic<DataToReclaim*> nodes_to_reclaim;

template <typename T>
DataToReclaim::DataToReclaim(T* p)
: data(p)
, deleter(&DoDelete<T>)
, next(0)
{}

DataToReclaim::~DataToReclaim() {
	deleter(data);
}

void AddToReclaimList(DataToReclaim* Node)
{
	Node->next = nodes_to_reclaim.load();
	while(nodes_to_reclaim.compare_exchange_weak(Node->next, Node))
		;
}

template <typename T>
void ReclaimLater(T* data)
{
	AddToReclaimList(new DataToReclaim(data));
}

void DeleteNodesWithNoHazards()
{
	DataToReclaim* current = nodes_to_reclaim.exchange(nullptr);
	while(current)
	{
		DataToReclaim* const next = current->next;
		if(!HasHazardPointerFor(current->data))
			delete current;
		else
			AddToReclaimList(current);
		current = next;
	}
}

template <typename T>
class LFstackHP
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

	atomic<Node*> head = {nullptr};
public:
	void push(T const& data);
	shared_ptr<T> pop();
	void show();
};

template <typename T>
void LFstackHP<T>::push(T const& data)
{
	Node *const new_node = new Node(data);
	new_node->next = head.load();
	while(!head.compare_exchange_weak(new_node->next, new_node))
		;
}

template <typename T>
shared_ptr<T> LFstackHP<T>::pop()
{
	atomic<void*> &hp = GetHazardPointerForCurrentThread();
	Node *old_head = head.load();
	do {
		Node *temp;
		do {
			temp = old_head;
			hp.store(old_head);
			old_head = head.load();
		}
		while(old_head != temp)
			;
	}
	while(old_head && !head.compare_exchange_strong(old_head, old_head->next))
		;
	hp.store(nullptr);

	shared_ptr<T> res;
	if(old_head)
	{
		res.swap(old_head->data);
		if(HasHazardPointerFor(old_head))
			ReclaimLater(old_head);
		else
			delete old_head;
		DeleteNodesWithNoHazards();
	}
	return res;
}

template <typename T>
void LFstackHP<T>::show()
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
	LFstackHP<int> lfstak;

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