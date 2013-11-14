#ifndef Map_h
#define Map_h

#include <map>
#include <rct/List.h>

template <typename Key, typename Value>
class Map : public std::map<Key, Value>
{
public:
    Map() {}

    bool contains(const Key &t) const
    {
        return std::map<Key, Value>::find(t) != std::map<Key, Value>::end();
    }

    bool isEmpty() const
    {
        return !std::map<Key, Value>::size();
    }

    Value value(const Key &key, const Value &defaultValue = Value()) const
    {
        typename std::map<Key, Value>::const_iterator it = std::map<Key, Value>::find(key);
        if (it == std::map<Key, Value>::end()) {
            return defaultValue;
        }
        return it->second;
    }

    Value take(const Key &key, bool *ok = 0)
    {
        Value ret = Value();
        if (remove(key, &ret)) {
            if (ok)
                *ok = true;
        } else if (ok) {
            *ok = false;
        }
        return ret;
    }

    bool remove(const Key &t, Value *value = 0)
    {
        typename std::map<Key, Value>::iterator it = std::map<Key, Value>::find(t);
        if (it != std::map<Key, Value>::end()) {
            if (value)
                *value = it->second;
            std::map<Key, Value>::erase(it);
            return true;
        }
        if (value)
            *value = Value();
        return false;
    }

    // bool insert(const Key &key, const Value &value)
    // {
    //     typedef typename std::map<Key, Value>::iterator Iterator;
    //     typedef std::pair<Iterator, bool> Tuple;
    //     Tuple tup = std::map<Key, Value>::insert(key, value);
    //     return std::map<Key, Value>::insert(key, value).second;
    //     // return tup->second;
    // }

    Value &operator[](const Key &key)
    {
        return std::map<Key, Value>::operator[](key);
    }

    const Value &operator[](const Key &key) const
    {
        assert(contains(key));
        return std::map<Key, Value>::find(key)->second;
    }

    Map<Key, Value> &unite(const Map<Key, Value> &other)
    {
        typename std::map<Key, Value>::const_iterator it = other.begin();
        while (it != other.end()) {
            const Key &key = it->first;
            const Value &val = it->second;
            std::map<Key, Value>::operator[](key) = val;
            // std::map<Key, Value>::insert(it);
            // std::map<Key, Value>::operator[](it->first) = it->second;
            ++it;
        }
        return *this;
    }

    Map<Key, Value> &subtract(const Map<Key, Value> &other)
    {
        typename std::map<Key, Value>::iterator it = other.begin();
        while (it != other.end()) {
            std::map<Key, Value>::erase(*it);
            ++it;
        }
        return *this;
    }

    Map<Key, Value> &operator+=(const Map<Key, Value> &other)
    {
        return unite(other);
    }

    Map<Key, Value> &operator-=(const Map<Key, Value> &other)
    {
        return subtract(other);
    }

    int size() const
    {
        return std::map<Key, Value>::size();
    }

    typename std::map<Key, Value>::const_iterator constBegin() const
    {
        return std::map<Key, Value>::begin();
    }

    typename std::map<Key, Value>::const_iterator constEnd() const
    {
        return std::map<Key, Value>::end();
    }

    List<Key> keys() const
    {
        List<Key> keys;
        typename std::map<Key, Value>::const_iterator it = std::map<Key, Value>::begin();
        while (it != std::map<Key, Value>::end()) {
            keys.append(it->first);
            ++it;
        }
        return keys;
    }

    Set<Key> keysAsSet() const
    {
        Set<Key> keys;
        typename std::map<Key, Value>::const_iterator it = std::map<Key, Value>::begin();
        while (it != std::map<Key, Value>::end()) {
            keys.insert(it->first);
            ++it;
        }
        return keys;
    }

    List<Value> values() const
    {
        List<Value> values;
        typename std::map<Key, Value>::const_iterator it = std::map<Key, Value>::begin();
        while (it != std::map<Key, Value>::end()) {
            values.append(it->second);
            ++it;
        }
        return values;
    }
};

template <typename Key, typename Value>
inline const Map<Key, Value> operator+(const Map<Key, Value> &l, const Map<Key, Value> &r)
{
    Map<Key, Value> ret = l;
    ret += r;
    return ret;
}

template <typename Key, typename Value>
inline const Map<Key, Value> operator-(const Map<Key, Value> &l, const Map<Key, Value> &r)
{
    Map<Key, Value> ret = l;
    ret -= r;
    return ret;
}

#endif
