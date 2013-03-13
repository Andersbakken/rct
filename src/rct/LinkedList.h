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
};

#endif
