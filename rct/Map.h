#ifndef Map_h
#define Map_h

#include <map>
#include <memory>

#include <rct/List.h>


template <typename Key, typename Value, typename Compare = std::less<Key> >
class Map : public std::map<Key, Value, Compare>
{
    typedef std::map<Key, Value, Compare> Base;
public:
    Map() {}
    Map(const Map<Key, Value, Compare> &other)
        : Base(other)
    {
    }

    Map(Map<Key, Value, Compare> &&other)
        : Base(std::move(other))
    {
    }

    Map(std::initializer_list<typename Base::value_type> init, const Compare& comp = Compare())
        : Base(init, comp)
    {
    }

    Map<Key, Value, Compare>& operator=(const Map<Key, Value, Compare>& other)
    {
        Base::operator=(other);
        return *this;
    }

    Map<Key, Value, Compare>& operator=(Map<Key, Value, Compare>&& other)
    {
        Base::operator=(std::move(other));
        return *this;
    }

    Map<Key, Value, Compare>& operator=(std::initializer_list<typename Base::value_type> init)
    {
        Base::operator=(init);
        return *this;
    }

    bool contains(const Key &t) const
    {
        return Base::find(t) != Base::end();
    }

    bool isEmpty() const
    {
        return !Base::size();
    }

    Value value(const Key &key, const Value &defaultValue, bool *ok = nullptr) const
    {
        typename Base::const_iterator it = Base::find(key);
        if (it == Base::end()) {
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
        typename Base::iterator it = Base::find(t);
        if (it != Base::end()) {
            if (val)
                *val = it->second;
            Base::erase(it);
            return true;
        }
        if (val)
            *val = Value();
        return false;
    }

    size_t remove(std::function<bool(const Key &key)> match)
    {
        size_t ret = 0;
        typename Base::iterator it = Base::begin();
        while (it != Base::end()) {
            if (match(it->first)) {
                Base::erase(it++);
                ++ret;
            } else {
                ++it;
            }
        }
        return ret;
    }

    void deleteAll()
    {
        typename Base::iterator it = Base::begin();
        while (it != Base::end()) {
            delete it.second;
            ++it;
        }
        Base::clear();
    }

    using Base::insert;
    bool insert(const Key &key, const Value &val)
    {
        return Base::insert(std::make_pair(key, val)).second;
    }

    Value &operator[](const Key &key)
    {
        return Base::operator[](key);
    }

    const Value &operator[](const Key &key) const
    {
        assert(contains(key));
        return Base::find(key)->second;
    }

    Map<Key, Value, Compare> &unite(const Map<Key, Value, Compare> &other, size_t *count = nullptr)
    {
        typename Base::const_iterator it = other.begin();
        const auto end = other.end();
        while (it != end) {
            const Key &key = it->first;
            const Value &val = it->second;
            if (count) {
                auto cur = Base::find(key);
                if (cur == end || cur->second != val) {
                    ++*count;
                    Base::operator[](key) = val;
                }
            } else {
                Base::operator[](key) = val;
            }
            ++it;
        }
        return *this;
    }

    Map<Key, Value, Compare> &subtract(const Map<Key, Value, Compare> &other)
    {
        typename Base::iterator it = other.begin();
        while (it != other.end()) {
            Base::erase(*it);
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

    size_t size() const
    {
        return Base::size();
    }

    typename Base::const_iterator constBegin() const
    {
        return Base::begin();
    }

    typename Base::const_iterator constEnd() const
    {
        return Base::end();
    }

    List<Key> keys() const
    {
        List<Key> k;
        k.reserve(size());
        typename Base::const_iterator it = Base::begin();
        while (it != Base::end()) {
            k.append(it->first);
            ++it;
        }
        return k;
    }

    Set<Key> keysAsSet() const
    {
        Set<Key> k;
        typename Base::const_iterator it = Base::begin();
        while (it != Base::end()) {
            k.insert(it->first);
            ++it;
        }
        return k;
    }

    List<Value> values() const
    {
        List<Value> vals;
        vals.reserve(size());
        typename Base::const_iterator it = Base::begin();
        while (it != Base::end()) {
            vals.append(it->second);
            ++it;
        }
        return vals;
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

    Value value(const Key &key, const Value &defaultValue = Value(), bool *ok = nullptr) const
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

    bool remove(const Key &t, Value *val = 0)
    {
        typename std::multimap<Key, Value, Compare>::iterator it = std::multimap<Key, Value, Compare>::find(t);
        if (it != std::multimap<Key, Value, Compare>::end()) {
            if (val)
                *val = it->second;
            std::multimap<Key, Value, Compare>::erase(it);
            return true;
        }
        if (val)
            *val = Value();
        return false;
    }

    size_t remove(std::function<bool(const Key &key)> match)
    {
        size_t ret = 0;
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

    using std::multimap<Key, Value>::insert;
    void insert(const Key &key, const Value &val)
    {
        std::multimap<Key, Value, Compare>::insert(std::make_pair<Key, Value>(key, val));
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

    MultiMap<Key, Value, Compare> &unite(const MultiMap<Key, Value, Compare> &other, size_t *count = nullptr)
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

    size_t size() const
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
        List<Key> k;
        k.reserve(size());
        typename std::multimap<Key, Value, Compare>::const_iterator it = std::multimap<Key, Value, Compare>::begin();
        while (it != std::multimap<Key, Value, Compare>::end()) {
            k.append(it->first);
            ++it;
        }
        return k;
    }

    Set<Key> keysAsSet() const
    {
        Set<Key> k;
        typename std::multimap<Key, Value, Compare>::const_iterator it = std::multimap<Key, Value, Compare>::begin();
        while (it != std::multimap<Key, Value, Compare>::end()) {
            k.insert(it->first);
            ++it;
        }
        return k;
    }

    List<Value> values() const
    {
        List<Value> vals;
        vals.reserve(size());
        typename std::multimap<Key, Value, Compare>::const_iterator it = std::multimap<Key, Value, Compare>::begin();
        while (it != std::multimap<Key, Value, Compare>::end()) {
            vals.append(it->second);
            ++it;
        }
        return vals;
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
