#ifndef DBstlmap_h
#define DBstlmap_h

#include "DB.h"
template <typename Key, typename Value>
struct DBPrivate
{


};

template <typename Key, typename Value>
DB<Key, Value>::DB()
    : mPrivate(0)
{
}

template <typename Key, typename Value>
DB<Key, Value>::~DB()
{
    delete mPrivate;
}

template <typename Key, typename Value>
bool DB<Key, Value>::load(const Path &path)
{

}

template <typename Key, typename Value>
std::shared_ptr<Value> DB<Key, Value>::value(const Key &key) const
{


}

template <typename Key, typename Value>
Key DB<Key, Value>::iterator::key() const
{

}

template <typename Key, typename Value>
std::shared_ptr<Value> DB<Key, Value>::iterator::value() const
{

}

template <typename Key, typename Value>
bool DB<Key, Value>::iterator::atEnd() const
{

}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator &DB<Key, Value>::iterator::operator++()
{

}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator &DB<Key, Value>::iterator::operator--()
{

}

template <typename Key, typename Value>
bool DB<Key, Value>::iterator::operator==(const iterator &other) const
{

}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::begin() const
{

}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::end() const
{

}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::lowerBound(const Key &key) const
{

}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::find(const Key &key) const
{

}

template <typename Key, typename Value>
DB<Key, Value>::WriteScope::WriteScope(DB &db)
{

}

template <typename Key, typename Value>
DB<Key, Value>::WriteScope::~WriteScope()
{

}
template <typename Key, typename Value>
int DB<Key, Value>::size() const
{

}

#endif
