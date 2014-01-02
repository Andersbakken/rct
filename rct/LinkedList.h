#ifndef LinkedList_h
#define LinkedList_h

#include <list>

template<typename T>
class LinkedList : public std::list<T>
{
public:
    LinkedList() : std::list<T>() { }
    LinkedList(int size) : std::list<T>(size) { }

    bool isEmpty() const { return std::list<T>::empty(); }

    int size() const { return std::list<T>::size(); }
    void append(const T &t) { std::list<T>::push_back(t); }
    void append(T &&t) { std::list<T>::push_back(std::move(t)); }
    void prepend(const T &t) { std::list<T>::push_front(t); }
    void prepend(T &&t) { std::list<T>::push_front(std::move(t)); }

    T &first() { return std::list<T>::front(); }
    const T &first() const { return std::list<T>::front(); }

    T &last() { return std::list<T>::back(); }
    const T &last() const { return std::list<T>::back(); }

    T takeFirst() { assert(!isEmpty()); const T t = first(); std::list<T>::pop_front(); return t; }
    T takeLast() { assert(!isEmpty()); const T t = last(); std::list<T>::pop_back(); return t; }
};

#endif
