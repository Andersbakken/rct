#ifndef Hash_h
#define Hash_h

#include <memory>
#include <unordered_map>

#include "List.h"

template <typename Key, typename Value>
class Hash : public std::unordered_map<Key, Value>
{
public:
    Hash() : std::unordered_map<Key, Value>() {}

    bool contains(const Key &t) const
    {
        return std::unordered_map<Key, Value>::find(t) != std::unordered_map<Key, Value>::end();
    }

    bool isEmpty() const
    {
        return !std::unordered_map<Key, Value>::size();
    }

    Value value(const Key &key, const Value &defaultValue, bool *ok = nullptr) const
    {
        typename std::unordered_map<Key, Value>::const_iterator it = std::unordered_map<Key, Value>::find(key);
        if (it == std::unordered_map<Key, Value>::end()) {
            if (ok)
                *ok = false;
            return defaultValue;
        }
        if (ok)
            *ok = true;
        return it->second;
    }

    Value value(const Key &key) const
    {
        return value(key, Value());
    }

    void deleteAll()
    {
        typename std::unordered_map<Key, Value>::iterator it = std::unordered_map<Key, Value>::begin();
        while (it != std::unordered_map<Key, Value>::end()) {
            delete it->second;
            ++it;
        }
        std::unordered_map<Key, Value>::clear();
    }

    Value take(const Key &key, bool *ok = nullptr)
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

    bool remove(const Key &t, Value *val = nullptr)
    {
        typename std::unordered_map<Key, Value>::iterator it = std::unordered_map<Key, Value>::find(t);
        if (it != std::unordered_map<Key, Value>::end()) {
            if (val)
                *val = it->second;
            std::unordered_map<Key, Value>::erase(it);
            return true;
        }
        if (val)
            *val = Value();
        return false;
    }


    size_t remove(std::function<bool(const Key &key)> match)
    {
        size_t ret = 0;
        typename std::unordered_map<Key, Value>::iterator it = std::unordered_map<Key, Value>::begin();
        while (it != std::unordered_map<Key, Value>::end()) {
            if (match(it->first)) {
                std::unordered_map<Key, Value>::erase(it++);
                ++ret;
            } else {
                ++it;
            }
        }
        return ret;
    }

    bool insert(const Key &key, const Value &val)
    {
        return std::unordered_map<Key, Value>::insert(std::make_pair(key, val)).second;
    }

    Value &operator[](const Key &key)
    {
        return std::unordered_map<Key, Value>::operator[](key);
    }

    const Value &operator[](const Key &key) const
    {
        assert(contains(key));
        return std::unordered_map<Key, Value>::find(key)->second;
    }

    Hash<Key, Value> &unite(const Hash<Key, Value> &other, size_t *count = nullptr)
    {
        typename std::unordered_map<Key, Value>::const_iterator it = other.begin();
        const auto end = other.end();
        while (it != end) {
            const Key &key = it->first;
            const Value &val = it->second;
            if (count) {
                auto cur = std::unordered_map<Key, Value>::find(key);
                if (cur == end || cur->second != val) {
                    ++*count;
                    std::unordered_map<Key, Value>::operator[](key) = val;
                }
            } else {
                std::unordered_map<Key, Value>::operator[](key) = val;
            }
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

    size_t size() const
    {
        return std::unordered_map<Key, Value>::size();
    }

    typename std::unordered_map<Key, Value>::const_iterator constBegin() const
    {
        return std::unordered_map<Key, Value>::begin();
    }

    typename std::unordered_map<Key, Value>::const_iterator constEnd() const
    {
        return std::unordered_map<Key, Value>::end();
    }

    List<Key> keys() const
    {
        List<Key> k;
        k.reserve(size());
        typename std::unordered_map<Key, Value>::const_iterator it = std::unordered_map<Key, Value>::begin();
        while (it != std::unordered_map<Key, Value>::end()) {
            k.append(it->first);
            ++it;
        }
        return k;
    }

    Set<Key> keysAsSet() const
    {
        Set<Key> k;
        typename std::unordered_map<Key, Value>::const_iterator it = std::unordered_map<Key, Value>::begin();
        while (it != std::unordered_map<Key, Value>::end()) {
            k.insert(it->first);
            ++it;
        }
        return k;
    }

    List<Value> values() const
    {
        List<Value> vals;
        vals.reserve(size());
        typename std::unordered_map<Key, Value>::const_iterator it = std::unordered_map<Key, Value>::begin();
        while (it != std::unordered_map<Key, Value>::end()) {
            vals.append(it->second);
            ++it;
        }
        return vals;
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
