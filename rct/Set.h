#ifndef Set_h
#define Set_h

#include <set>
#include <memory>

#include <rct/List.h>

template <typename T>
class Set : public std::set<T>
{
public:
    typedef std::set<T> Base;
    Set() {}

    bool contains(const T &t) const
    {
        return Base::find(t) != Base::end();
    }

    bool isEmpty() const
    {
        return !Base::size();
    }

    bool remove(const T &t)
    {
        typename Base::iterator it = Base::find(t);
        if (it != Base::end()) {
            Base::erase(it);
            return true;
        }
        return false;
    }
    size_t remove(std::function<bool(const T &t)> match)
    {
        size_t ret = 0;
        typename Base::iterator it = Base::begin();
        while (it != Base::end()) {
            if (match(*it)) {
                Base::erase(it++);
                ++ret;
            } else {
                ++it;
            }
        }
        return ret;
    }

    List<T> toList() const
    {
        List<T> ret(size());
        typename Base::const_iterator it = Base::begin();
        size_t i = 0;
        while (it != Base::end()) {
            ret[i++] = *it;
            ++it;
        }
        return ret;
    }

    void deleteAll()
    {
        typename Base::iterator it = Base::begin();
        while (it != Base::end()) {
            delete *it;
            ++it;
        }
        Base::clear();
    }

    bool insert(const T &t)
    {
        return Base::insert(t).second;
    }

    Set<T> &unite(const Set<T> &other, size_t *count = nullptr)
    {
        size_t c = 0;
        if (isEmpty()) {
            *this = other;
            c = other.size();
        } else {
            typename Base::const_iterator it = other.begin();
            while (it != other.end()) {
                if (insert(*it))
                    ++c;
                ++it;
            }
        }
        if (count)
            *count = c;
        return *this;
    }

    bool intersects(const Set<T>& other) const
    {
        typename Base::const_iterator it = other.begin();
        const typename Base::const_iterator end = other.end();
        while (it != end) {
            if (contains(*it))
                return true;
            ++it;
        }
        return false;
    }

    Set<T> intersected(const Set<T>& other) const
    {
        Set<T> ret;
        typename Base::const_iterator it = other.begin();
        const typename Base::const_iterator end = other.end();
        while (it != end) {
            if (contains(*it))
                ret.insert(*it);
            ++it;
        }
        return ret;
    }

    Set<T> &unite(const List<T> &other, size_t *count = nullptr)
    {
        size_t c = 0;
        typename std::vector<T>::const_iterator it = other.begin();
        while (it != other.end()) {
            if (insert(*it))
                ++c;
            ++it;
        }
        if (count)
            *count = c;
        return *this;
    }

    Set<T> &subtract(const Set<T> &other, size_t *count = nullptr)
    {
        size_t c = 0;
        if (!isEmpty()) {
            typename Base::const_iterator it = other.begin();
            while (it != other.end()) {
                c += Base::erase(*it);
                ++it;
            }
        }
        if (count)
            *count = c;
        return *this;
    }

    Set<T> &operator+=(const Set<T> &other)
    {
        return unite(other);
    }

    Set<T> &operator+=(const T &t)
    {
        insert(t);
        return *this;
    }

    Set<T> &operator+=(const List<T> &other)
    {
        return unite(other);
    }

    Set<T> &operator<<(const T &t)
    {
        insert(t);
        return *this;
    }
    Set<T> &operator<<(const List<T> &t)
    {
        return unite(t);
    }

    Set<T> &operator<<(const Set<T> &t)
    {
        return unite(t);
    }

    Set<T> &operator-=(const Set<T> &other)
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

    template <typename K>
    size_t compare(const Set<K> &other) const
    {
        const size_t me = size();
        const size_t him = other.size();
        if (me < him) {
            return -1;
        } else if (me > him) {
            return 1;
        }
        typename Set<K>::const_iterator bit = other.begin();
        for (typename Set<T>::const_iterator it = Base::begin(); it != Base::end(); ++it) {
            const size_t cmp = it->compare(*bit);
            if (cmp)
                return cmp;
            ++bit;
        }
        return 0;
    }

    template <typename K>
    bool operator==(const Set<K> &other) const
    {
        return !compare(other);
    }

    template <typename K>
    bool operator!=(const Set<K> &other) const
    {
        return compare(other);
    }

    template <typename K>
    bool operator<(const Set<K> &other) const
    {
        return compare(other) < 0;
    }

    template <typename K>
    bool operator>(const Set<K> &other) const
    {
        return compare(other) > 0;
    }
};

template <typename T>
inline const Set<T> operator+(const Set<T> &l, const Set<T> &r)
{
    Set<T> ret = l;
    ret += r;
    return ret;
}

template <typename T>
inline const Set<T> operator-(const Set<T> &l, const Set<T> &r)
{
    Set<T> ret = l;
    ret -= r;
    return ret;
}

template <typename T>
Set<T> List<T>::toSet() const
{
    Set<T> ret;
    const size_t s = size();
    for (size_t i=0; i<s; ++i) {
        ret.insert(std::vector<T>::at(i));
    }
    return ret;
}


#endif
