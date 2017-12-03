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

		atomic<void*>& GetPointer() { return hp->pointer; }
	};

	atomic<void*>& GetHazardPointerForCurrentThread();
	bool HasHazardPointerFor(void*p);

	template <typename T>
	void DoDelete(void* p);

	struct DataToReclaim {
		void *data;
		function<void(void*)> deleter;
		DataToReclaim *next;

		template <typename T>
		DataToReclaim(T* p);

		~DataToReclaim();
	};
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
atomic<void*>&  LFStackHP<T>::GetHazardPointerForCurrentThread()
{
	thread_local static hpOwner hazard;
	return hazard.getPointer();
}

template <typename T>
bool LFStackHP<T>::HasHazardPointerFor(void* p)
{
	for (unsigned i = 0; i < MAX_HAZARD_POINTERS; i++)
		if (hazardPointers[i].pointer.load() == p)
			return true;
	return false;
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
	atomic<void*>& hp = GetHazardPointerForCurrentThread();
	Node* oldHead = head.load();

	do {
		Node* temp;
		do {
			tmp = oldHead;
			hp.store(oldHead);
			oldHead = head.load();
		} while (oldHead != temp)
			;
	} while (oldHead && !head.compare_exchange_strong(oldHead, oldHead->next))
		;

	hp.store(nullptr);
	shared_ptr<T> res;
	if (oldHead) {
		res.swap(oldHead->data);
		if (HasHazardPointerFor(oldHead))
			ReclaimLater(oldHead);
		else
			delete oldHead;
		DeleteNodesWithNotHazards();
	}
	return res;
}

template <typename T>
void LFStackHP<T>::DoDelete(void* p) {
	delete static_cast<T*>(p);
}

template <typename T>
LFStackHP<T>::DataToReclaim::DataToReclaim(void* p)
: data(p)
, deleter(&DoDelete<T>)
, next(nullptr)
{}

template <typename T>
LFStackHP<T>::DataToReclaim::~DataToReclaim() {
	deleter(data);
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