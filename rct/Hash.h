#ifndef Hash_h
#define Hash_h

#include <unordered_map>
#include <rct/List.h>

template <typename Key, typename Value>
class Hash : public std::unordered_map<Key, Value>
{
public:
    Hash() {}

    bool contains(const Key &t) const
    {
        return std::unordered_map<Key, Value>::find(t) != std::unordered_map<Key, Value>::end();
    }

    bool isEmpty() const
    {
        return !std::unordered_map<Key, Value>::size();
    }

    Value value(const Key &key, const Value &defaultValue = Value()) const
    {
        typename std::unordered_map<Key, Value>::const_iterator it = std::unordered_map<Key, Value>::find(key);
        if (it == std::unordered_map<Key, Value>::end()) {
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
        typename std::unordered_map<Key, Value>::iterator it = std::unordered_map<Key, Value>::find(t);
        if (it != std::unordered_map<Key, Value>::end()) {
            if (value)
                *value = it->second;
            std::unordered_map<Key, Value>::erase(it);
            return true;
        }
        if (value)
            *value = Value();
        return false;
    }

    // bool insert(const Key &key, const Value &value)
    // {
    //     typedef typename std::unordered_map<Key, Value>::iterator Iterator;
    //     typedef std::pair<Iterator, bool> Tuple;
    //     Tuple tup = std::unordered_map<Key, Value>::insert(key, value);
    //     return std::unordered_map<Key, Value>::insert(key, value).second;
    //     // return tup->second;
    // }

    Value &operator[](const Key &key)
    {
        return std::unordered_map<Key, Value>::operator[](key);
    }

    const Value &operator[](const Key &key) const
    {
        assert(contains(key));
        return std::unordered_map<Key, Value>::find(key)->second;
    }

    Hash<Key, Value> &unite(const Hash<Key, Value> &other)
    {
        typename std::unordered_map<Key, Value>::const_iterator it = other.begin();
        while (it != other.end()) {
            const Key &key = it->first;
            const Value &val = it->second;
            std::unordered_map<Key, Value>::operator[](key) = val;
            // std::unordered_map<Key, Value>::insert(it);
            // std::unordered_map<Key, Value>::operator[](it->first) = it->second;
            ++it;
        }
        return *this;
    }

    Hash<Key, Value> &subtract(const Hash<Key, Value> &other)
    {
        typename std::unordered_map<Key, Value>::iterator it = other.begin();
        while (it != other.end()) {
            std::unordered_map<Key, Value>::erase(*it);
            ++it;
        }
        return *this;
    }

    Hash<Key, Value> &operator+=(const Hash<Key, Value> &other)
    {
        return unite(other);
    }

    Hash<Key, Value> &operator-=(const Hash<Key, Value> &other)
    {
        return subtract(other);
    }

    int size() const
    {
        return std::unordered_map<Key, Value>::size();
    }

    List<Key> keys() const
    {
        List<Key> keys;
        typename std::unordered_map<Key, Value>::const_iterator it = std::unordered_map<Key, Value>::begin();
        while (it != std::unordered_map<Key, Value>::end()) {
            keys.append(it->first);
            ++it;
        }
        return keys;
    }

    Set<Key> keysAsSet() const
    {
        Set<Key> keys;
        typename std::unordered_map<Key, Value>::const_iterator it = std::unordered_map<Key, Value>::begin();
        while (it != std::unordered_map<Key, Value>::end()) {
            keys.insert(it->first);
            ++it;
        }
        return keys;
    }

    List<Value> values() const
    {
        List<Value> values;
        typename std::unordered_map<Key, Value>::const_iterator it = std::unordered_map<Key, Value>::begin();
        while (it != std::unordered_map<Key, Value>::end()) {
            values.append(it->second);
            ++it;
        }
        return values;
    }
};

template <typename Key, typename Value>
inline const Hash<Key, Value> operator+(const Hash<Key, Value> &l, const Hash<Key, Value> &r)
{
    Hash<Key, Value> ret = l;
    ret += r;
    return ret;
}

template <typename Key, typename Value>
inline const Hash<Key, Value> operator-(const Hash<Key, Value> &l, const Hash<Key, Value> &r)
{
    Hash<Key, Value> ret = l;
    ret -= r;
    return ret;
}

#endif
