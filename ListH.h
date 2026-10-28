#pragma once

#ifndef LISTH_H
#define LISTH_H

#include <iostream>
#include <string>
#include <stdexcept>
template <typename T>
struct Node
{
	T data;
	Node* next;
};

template <typename T>
class LinkedList {
public:
	LinkedList();
	LinkedList(const T arr[], int n);
	~LinkedList();                                                             //析构函数
	LinkedList<T>& operator=(const LinkedList<T>& other);                      //深拷贝重载函数
	void Insert(int i, const T& x);                                            //按位置插入
	void PushBack(const T& data);                                              //提交到尾部
	void PrintLink();                                                          //循环打印
	void Clear();                                                              //清空链表
	T Delete(int i);                                                           //按位置删除
	T Get(int i);                                                              //查找位置返回数据
	T& GetRef(int i);                                                          //返回数据引用，可原地修改
	Node<T>* GetHand();                                                        //返回第一个数据节点
	int Locate(const T& x);                                                    //查找数据返回位置
	int GetLength();                                                           //返回链表长度
	bool Empty();                                                              //链表判空
private:
	Node<T>* first;
	Node<T>* last;
	int length;
};

template <typename T>
LinkedList<T>::LinkedList() : first(new Node<T>{}),last(first), length(0) {
	first->next = nullptr;
}

template <typename T>
LinkedList<T>::LinkedList(const T arr[], int n) : first(new Node<T>{}),last(first), length(0) {
	first->next = nullptr;

	Node<T>* r = first;
	for (int i = 0; i < n; i++) {
		Node<T>* s = new Node<T>;
		s->data = arr[i];
		s->next = nullptr;

		r->next = s;
		r = s;
		length++;
	}
	last = r;
}

template <typename T>
LinkedList<T>::~LinkedList() {                                                 //析构函数
	Node<T>* p = first;
	while (p != nullptr) {
		Node<T>* temp = p;
		p = p->next;
		delete temp;
	}

	first = nullptr;
	last = nullptr;
}
template <typename T>
LinkedList<T>& LinkedList<T>::operator=(const LinkedList<T>& other) {
	if (this != &other) {
		Clear();
		Node<T>* p = other.first->next;
		while (p != nullptr) {
			this->PushBack(p->data);
			p = p->next;
		}
	}
	return *this;
}

template <typename T>
void LinkedList<T>::Insert(int i, const T& x) {                                //按位置插入
	if (i<=0 || i>length+1) {
		std::cerr << "error" << std::endl;
		return;
	}

	Node<T>* s = new Node<T>;
	s->data = x;
	s->next = nullptr;

	Node<T>* p = first;
	int count = 0;
	while (p != nullptr && count < i - 1) {
		p = p->next;
		count++;
	}
	if (p != nullptr) {
		s->next = p->next;
		p->next = s;

		if (p == last) {
			last = s;
		}

		length++;
	}
	else {
		std::cerr << "error" << std::endl;
		delete s;
	}
}

template <typename T>
void LinkedList<T>::PushBack(const T& data) {                                  //提交到尾部
	Node<T>* s = new Node<T>;
	s->data = data;
	s->next = nullptr;

	last->next = s; // 把新节点挂到当前尾节点后面
	last = s;       // 更新尾指针，让它指向新的最后一个节点
	length++;
}

template <typename T>
T LinkedList<T>::Delete(int i) {                                               //按位置删除
	if (i <= 0 || i >= length) {
		std::cerr << "error" << std::endl;
		return T{};
	}

	Node<T>* p = first;
	int count = 0;
	while (p != nullptr && count < i - 1) {
		p = p->next;
		count++;
	}

	if (p != nullptr && p->next != nullptr) {      //delete next
		Node<T>* s = p->next;
		T x = s->data;

		if (s == last) {
			last = p;  // 如果是尾节点，把尾指针退回到它的前驱节点
		}

		p->next = s->next;  // 2. 摘下节点
		delete s;           // 3. 释放内存
		length--;           // 4. 长度减 1

		// 5. 如果删空了，确保 last 归位到 first
		if (length == 0) {
			last = first;
		}

		return x;
	}
	else {
		std::cerr << "error" << std::endl;
		return T{};
	}
}

template <typename T>
int LinkedList<T>::Locate(const T& x) {                                        //查找数据返回位置
	if (Empty()) return -1;
	Node<T>* p = first->next;

	for (int i = 0; i < length; i++) {   //index
		if (p->data == x) {
			return i + 1;
		}
		p = p->next;
	}
	return -1;
}

template <typename T>
T LinkedList<T>::Get(int i) {                                                  //查找位置返回数据

	if (i <= 0 || i > length) {
		std::cerr << "error" << std::endl;
		return T{};
	}
	Node<T>* p = first->next;
	int count = 1;

	while (!Empty() && count < i) {
		p = p->next;
		count++;
	}

	return p->data;
}

template <typename T>
T& LinkedList<T>::GetRef(int i) {                                              //返回数据引用
	if (i <= 0 || i > length) {
		throw std::out_of_range("LinkedList::GetRef: index out of range");
	}
	Node<T>* p = first->next;
	int count = 1;

	while (count < i) {
		p = p->next;
		count++;
	}

	return p->data;
}

template <typename T>
int LinkedList<T>::GetLength() { return length; }                              //返回链表长度

template <typename T>
void LinkedList<T>::PrintLink() {                                              //遍历链表
	int count = 0;
	Node<T>* p = first->next;
	while (!Empty() && count < length) {
		std::cout << p->data << " ";
		p = p->next;
		count++;
	}
	std::cout << std::endl << std::endl;
}

template <typename T>
bool LinkedList<T>::Empty() {                                                  //链表判空
	return length == 0;
}

template <typename T>
Node<T>* LinkedList<T>::GetHand() {                                            //返回第一个数据节点
	return first->next;
}

template <typename T>
void LinkedList<T>::Clear() {                                            
	Node<T>* p = first->next;
	while (p != nullptr) {
		Node<T>* temp = p;
		p = p->next;
		delete p;
	}
	first->next = nullptr;
	last = first;
	length = 0;
}
#endif // !LISTH_H