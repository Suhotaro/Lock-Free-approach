#include "stdafx.h"
#include <iostream>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>

using namespace std;

template <typename T>
class LFStackASP
{
private:
	struct Node
	{
		shared_ptr<T> data;
		shared_ptr<Node> next;
		Node(T const& data_) : data(make_shared<T>(data_)) {}
	};
	shared_ptr<Node> head;
public:
	void push(T const& data);
	shared_ptr<T> pop();
	void show();
};

template <typename T>
void LFStackASP<T>::push(T const& data)
{
	shared_ptr<Node> const newNode = make_shared<Node>(data);
	newNode->next = atomic_load(&head);
	while(!atomic_compare_exchange_weak(&head, &newNode->next, newNode))
		;
}

template <typename T>
shared_ptr<T> LFStackASP<T>::pop()
{
	shared_ptr<Node> oldHead = atomic_load(&head);
	while(oldHead && !atomic_compare_exchange_weak(&head, &oldHead, oldHead->next))
		;
	return oldHead ? oldHead->data : shared_ptr<T>();
}

template <typename T>
void LFStackASP<T>::show()
{
	shared_ptr<Node> n = atomic_load(&head);
	while(n)
	{
		printf("%d \n", *n->data);
		n = n->next;
	}
}

int main()
{
	LFStackASP<int> lfstak;

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