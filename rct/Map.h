#ifndef Map_h
#define Map_h

#include <map>
#include <rct/List.h>
#include <memory>

template <typename Key, typename Value, typename Compare = std::less<Key> >
class Map : public std::map<Key, Value, Compare>
{
public:
    Map() {}
    Map(std::initializer_list<typename std::map<Key, Value>::value_type> init, const Compare& comp = Compare())
        : std::map<Key, Value, Compare>(init, comp)
    {
    }

    Map<Key, Value, Compare>& operator=(const Map<Key, Value, Compare>& other)
    {
        std::map<Key, Value, Compare>::operator=(other);
        return *this;
    }

    Map<Key, Value, Compare>& operator=(std::initializer_list<typename std::map<Key, Value>::value_type> init)
    {
        std::map<Key, Value, Compare>::operator=(init);
        return *this;
    }

    bool contains(const Key &t) const
    {
        return std::map<Key, Value, Compare>::find(t) != std::map<Key, Value, Compare>::end();
    }

    bool isEmpty() const
    {
        return !std::map<Key, Value, Compare>::size();
    }

    Value value(const Key &key, const Value &defaultValue = Value(), bool *ok = 0) const
    {
        typename std::map<Key, Value, Compare>::const_iterator it = std::map<Key, Value, Compare>::find(key);
        if (it == std::map<Key, Value, Compare>::end()) {
            if (ok)
                *ok = false;
            return defaultValue;
        }
        if (ok)
            *ok = true;
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
        typename std::map<Key, Value, Compare>::iterator it = std::map<Key, Value, Compare>::find(t);
        if (it != std::map<Key, Value, Compare>::end()) {
            if (value)
                *value = it->second;
            std::map<Key, Value, Compare>::erase(it);
            return true;
        }
        if (value)
            *value = Value();
        return false;
    }

    int remove(std::function<bool(const Key &key)> match)
    {
        int ret = 0;
        typename std::map<Key, Value, Compare>::iterator it = std::map<Key, Value, Compare>::begin();
        while (it != std::map<Key, Value, Compare>::end()) {
            if (match(it->first)) {
                std::map<Key, Value, Compare>::erase(it++);
                ++ret;
            } else {
                ++it;
            }
        }
        return ret;
    }

    void deleteAll()
    {
        typename std::map<Key, Value, Compare>::iterator it = std::map<Key, Value, Compare>::begin();
        while (it != std::map<Key, Value, Compare>::end()) {
            delete it.second;
            ++it;
        }
        std::map<Key, Value, Compare>::clear();
    }

    bool insert(const Key &key, const Value &value)
    {
        return std::map<Key, Value, Compare>::insert(std::make_pair(key, value)).second;
    }

    Value &operator[](const Key &key)
    {
        return std::map<Key, Value, Compare>::operator[](key);
    }

    const Value &operator[](const Key &key) const
    {
        assert(contains(key));
        return std::map<Key, Value, Compare>::find(key)->second;
    }

    Map<Key, Value, Compare> &unite(const Map<Key, Value, Compare> &other, int *count = 0)
    {
        typename std::map<Key, Value, Compare>::const_iterator it = other.begin();
        const auto end = other.end();
        while (it != end) {
            const Key &key = it->first;
            const Value &val = it->second;
            if (count) {
                auto cur = std::map<Key, Value, Compare>::find(key);
                if (cur == end || cur->second != val) {
                    ++*count;
                    std::map<Key, Value, Compare>::operator[](key) = val;
                }
            } else {
                std::map<Key, Value, Compare>::operator[](key) = val;
            }
            ++it;
        }
        return *this;
    }

    Map<Key, Value, Compare> &subtract(const Map<Key, Value, Compare> &other)
    {
        typename std::map<Key, Value, Compare>::iterator it = other.begin();
        while (it != other.end()) {
            std::map<Key, Value, Compare>::erase(*it);
            ++it;
        }
        return *this;
    }

    Map<Key, Value, Compare> &operator+=(const Map<Key, Value, Compare> &other)
    {
        return unite(other);
    }

    Map<Key, Value, Compare> &operator-=(const Map<Key, Value, Compare> &other)
    {
        return subtract(other);
    }

    int size() const
    {
        return std::map<Key, Value, Compare>::size();
    }

    typename std::map<Key, Value, Compare>::const_iterator constBegin() const
    {
        return std::map<Key, Value, Compare>::begin();
    }

    typename std::map<Key, Value, Compare>::const_iterator constEnd() const
    {
        return std::map<Key, Value, Compare>::end();
    }

    List<Key> keys() const
    {
        List<Key> keys;
        typename std::map<Key, Value, Compare>::const_iterator it = std::map<Key, Value, Compare>::begin();
        while (it != std::map<Key, Value, Compare>::end()) {
            keys.append(it->first);
            ++it;
        }
        return keys;
    }

    Set<Key> keysAsSet() const
    {
        Set<Key> keys;
        typename std::map<Key, Value, Compare>::const_iterator it = std::map<Key, Value, Compare>::begin();
        while (it != std::map<Key, Value, Compare>::end()) {
            keys.insert(it->first);
            ++it;
        }
        return keys;
    }

    List<Value> values() const
    {
        List<Value> values;
        typename std::map<Key, Value, Compare>::const_iterator it = std::map<Key, Value, Compare>::begin();
        while (it != std::map<Key, Value, Compare>::end()) {
            values.append(it->second);
            ++it;
        }
        return values;
    }
};

template <typename Key, typename Value, typename Compare>
inline const Map<Key, Value, Compare> operator+(const Map<Key, Value, Compare> &l, const Map<Key, Value, Compare> &r)
{
    Map<Key, Value, Compare> ret = l;
    ret += r;
    return ret;
}

template <typename Key, typename Value, typename Compare>
inline const Map<Key, Value, Compare> operator-(const Map<Key, Value, Compare> &l, const Map<Key, Value, Compare> &r)
{
    Map<Key, Value, Compare> ret = l;
    ret -= r;
    return ret;
}


template <typename Key, typename Value, typename Compare = std::less<Key> >
class MultiMap : public std::multimap<Key, Value, Compare>
{
public:
    MultiMap() {}
    MultiMap(std::initializer_list<typename std::multimap<Key, Value>::value_type> init, const Compare& comp = Compare())
        : std::multimap<Key, Value, Compare>(init, comp)
    {
    }

    MultiMap<Key, Value, Compare>& operator=(const MultiMap<Key, Value, Compare>& other)
    {
        std::multimap<Key, Value, Compare>::operator=(other);
        return *this;
    }

    MultiMap<Key, Value, Compare>& operator=(std::initializer_list<typename std::multimap<Key, Value>::value_type> init)
    {
        std::multimap<Key, Value, Compare>::operator=(init);
        return *this;
    }

    bool contains(const Key &t) const
    {
        return std::multimap<Key, Value, Compare>::find(t) != std::multimap<Key, Value, Compare>::end();
    }

    bool isEmpty() const
    {
        return !std::multimap<Key, Value, Compare>::size();
    }

    Value value(const Key &key, const Value &defaultValue = Value(), bool *ok = 0) const
    {
        typename std::multimap<Key, Value, Compare>::const_iterator it = std::multimap<Key, Value, Compare>::find(key);
        if (it == std::multimap<Key, Value, Compare>::end()) {
            if (ok)
                *ok = false;
            return defaultValue;
        }
        if (ok)
            *ok = true;
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
        typename std::multimap<Key, Value, Compare>::iterator it = std::multimap<Key, Value, Compare>::find(t);
        if (it != std::multimap<Key, Value, Compare>::end()) {
            if (value)
                *value = it->second;
            std::multimap<Key, Value, Compare>::erase(it);
            return true;
        }
        if (value)
            *value = Value();
        return false;
    }

    int remove(std::function<bool(const Key &key)> match)
    {
        int ret = 0;
        typename std::multimap<Key, Value, Compare>::iterator it = std::multimap<Key, Value, Compare>::begin();
        while (it != std::multimap<Key, Value, Compare>::end()) {
            if (match(it->first)) {
                std::multimap<Key, Value, Compare>::erase(it++);
                ++ret;
            } else {
                ++it;
            }
        }
        return ret;
    }

    void deleteAll()
    {
        typename std::multimap<Key, Value, Compare>::iterator it = std::multimap<Key, Value, Compare>::begin();
        while (it != std::multimap<Key, Value, Compare>::end()) {
            delete it.second;
            ++it;
        }
        std::multimap<Key, Value, Compare>::clear();
    }

    bool insert(const Key &key, const Value &value)
    {
        return std::multimap<Key, Value, Compare>::insert(std::make_pair(key, value)).second;
    }

    Value &operator[](const Key &key)
    {
        return std::multimap<Key, Value, Compare>::operator[](key);
    }

    const Value &operator[](const Key &key) const
    {
        assert(contains(key));
        return std::multimap<Key, Value, Compare>::find(key)->second;
    }

    MultiMap<Key, Value, Compare> &unite(const MultiMap<Key, Value, Compare> &other, int *count = 0)
    {
        typename std::multimap<Key, Value, Compare>::const_iterator it = other.begin();
        const auto end = other.end();
        while (it != end) {
            const Key &key = it->first;
            const Value &val = it->second;
            if (count) {
                auto cur = std::multimap<Key, Value, Compare>::find(key);
                if (cur == end || cur->second != val) {
                    ++*count;
                    std::multimap<Key, Value, Compare>::operator[](key) = val;
                }
            } else {
                std::multimap<Key, Value, Compare>::operator[](key) = val;
            }
            ++it;
        }
        return *this;
    }

    MultiMap<Key, Value, Compare> &subtract(const MultiMap<Key, Value, Compare> &other)
    {
        typename std::multimap<Key, Value, Compare>::iterator it = other.begin();
        while (it != other.end()) {
            std::multimap<Key, Value, Compare>::erase(*it);
            ++it;
        }
        return *this;
    }

    MultiMap<Key, Value, Compare> &operator+=(const MultiMap<Key, Value, Compare> &other)
    {
        return unite(other);
    }

    MultiMap<Key, Value, Compare> &operator-=(const MultiMap<Key, Value, Compare> &other)
    {
        return subtract(other);
    }

    int size() const
    {
        return std::multimap<Key, Value, Compare>::size();
    }

    typename std::multimap<Key, Value, Compare>::const_iterator constBegin() const
    {
        return std::multimap<Key, Value, Compare>::begin();
    }

    typename std::multimap<Key, Value, Compare>::const_iterator constEnd() const
    {
        return std::multimap<Key, Value, Compare>::end();
    }

    List<Key> keys() const
    {
        List<Key> keys;
        typename std::multimap<Key, Value, Compare>::const_iterator it = std::multimap<Key, Value, Compare>::begin();
        while (it != std::multimap<Key, Value, Compare>::end()) {
            keys.append(it->first);
            ++it;
        }
        return keys;
    }

    Set<Key> keysAsSet() const
    {
        Set<Key> keys;
        typename std::multimap<Key, Value, Compare>::const_iterator it = std::multimap<Key, Value, Compare>::begin();
        while (it != std::multimap<Key, Value, Compare>::end()) {
            keys.insert(it->first);
            ++it;
        }
        return keys;
    }

    List<Value> values() const
    {
        List<Value> values;
        typename std::multimap<Key, Value, Compare>::const_iterator it = std::multimap<Key, Value, Compare>::begin();
        while (it != std::multimap<Key, Value, Compare>::end()) {
            values.append(it->second);
            ++it;
        }
        return values;
    }
};

template <typename Key, typename Value, typename Compare>
inline const MultiMap<Key, Value, Compare> operator+(const MultiMap<Key, Value, Compare> &l, const MultiMap<Key, Value, Compare> &r)
{
    MultiMap<Key, Value, Compare> ret = l;
    ret += r;
    return ret;
}

template <typename Key, typename Value, typename Compare>
inline const MultiMap<Key, Value, Compare> operator-(const MultiMap<Key, Value, Compare> &l, const MultiMap<Key, Value, Compare> &r)
{
    MultiMap<Key, Value, Compare> ret = l;
    ret -= r;
    return ret;
}

#endif
