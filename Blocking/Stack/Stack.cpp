#include "stdafx.h"
#include <iostream>
#include <mutex>
#include <stack>

using namespace std;

template<typename T>
class ThreadsafeStack
{
private:
	stack<T> data;
	mutable mutex m;
public:
	ThreadsafeStack() = default;
	ThreadsafeStack(const ThreadsafeStack& other);
	ThreadsafeStack& operator=(const ThreadsafeStack&) = delete;

	void push(T new_value);
	shared_ptr<T> pop();
	bool empty() const;
};

template <typename T>
ThreadsafeStack<T>::ThreadsafeStack(const ThreadsafeStack& other)
{
	lock_guard<mutex> lock(other.m);
	data = other.data;
}

template <typename T>
void ThreadsafeStack<T>::push(T newValue)
{
	lock_guard<mutex> lock(m);
	data.push(move(newValue));
}

template <typename T>
shared_ptr<T> ThreadsafeStack<T>::pop()
{
	lock_guard<mutex> lock(m);
	shared_ptr<T> const res;
	if(!data.empty())
	{
		res = make_shared<T>(move(data.top));
		data.pop();
	}
	return res;
}

template <typename T>
bool ThreadsafeStack<T>::empty() const
{
	lock_guard<mutex> lock(m);
	return data.empty();
}

int main()
{
	return 0;
}