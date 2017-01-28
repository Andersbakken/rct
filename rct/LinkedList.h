#ifndef LinkedList_h
#define LinkedList_h

#include <algorithm>
#include <list>

template<typename T>
class LinkedList : public std::list<T>
{
public:
    LinkedList() : std::list<T>() { }
    LinkedList(size_t len) : std::list<T>(len) { }

    bool isEmpty() const { return std::list<T>::empty(); }

    size_t size() const { return std::list<T>::size(); }
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

    bool contains(const T& t) const { return std::find(std::list<T>::begin(), std::list<T>::end(), t) != std::list<T>::end(); }

    typename std::list<T>::iterator find(const T &t)
    {
        for (auto it = std::list<T>::begin(); it != std::list<T>::end(); ++it) {
            if (*it == t)
                return it;
        }
        return std::list<T>::end();
    }

    typename std::list<T>::const_iterator find(const T &t) const
    {
        for (auto it = std::list<T>::begin(); it != std::list<T>::end(); ++it) {
            if (*it == t)
                return it;
        }
        return std::list<T>::end();
    }

    void deleteAll()
    {
        typename std::list<T>::iterator it = std::list<T>::begin();
        while (it != std::list<T>::end()) {
            delete *it;
            ++it;
        }
        std::list<T>::clear();
    }
};

#endif
