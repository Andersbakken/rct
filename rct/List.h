#ifndef List_h
#define List_h

#include <vector>
#include <algorithm>
#include <assert.h>

template <typename T> class Set;

template <typename T>
class List : public std::vector<T>
{
    typedef std::vector<T> Base;
public:
    explicit List(int count = 0, const T &defaultValue = T())
        : Base(count, defaultValue)
    {}

    template <typename CompatibleType>
    List(const std::vector<CompatibleType> &other)
        : Base(other.size(), T())
    {
        const int size = other.size();
        for (int i=0; i<size; ++i) {
            std::vector<T>::operator[](i) = other.at(i);
        }
    }

    bool contains(const T &t) const
    {
        return std::find(Base::begin(), Base::end(), t) != Base::end();
    }

    bool isEmpty() const
    {
        return Base::empty();
    }

    void append(const T &t)
    {
        Base::push_back(t);
    }

    void prepend(const T &t)
    {
        Base::insert(Base::begin(), t);
    }

    void append(const List<T> &t)
    {
        const int size = t.size();
        for (int i=0; i<size; ++i)
            Base::push_back(t.at(i));
    }

    int indexOf(const T &t) const
    {
        const typename Base::const_iterator beg = Base::begin();
        const typename Base::const_iterator end = Base::end();
        const typename Base::const_iterator it = std::find(beg, end, t);
        return it == end ? -1 : (it - beg);
    }

    int lasIndexOf(const T &t, int from = -1) const
    {
        const int s = size();
        if (from < 0) {
            from += s;
        }
        from = std::min(s - 1, from);
        if (from >= 0) {
            const T *haystack = Base::constData();
            const T *needle = haystack + from + 1;
            while (needle != haystack) {
                if (*--needle == t)
                    return needle - haystack;
            }
        }
        return -1;
    }

    void removeAt(int idx)
    {
        Base::erase(Base::begin() + idx);
    }

    void removeLast()
    {
        Base::pop_back();
    }

    void removeFirst()
    {
        Base::pop_front();
    }

    int size() const
    {
        return Base::size();
    }

    T value(int idx, const T &defaultValue = T()) const
    {
        return static_cast<unsigned int>(idx) < Base::size() ? Base::at(idx) : defaultValue;
    }

    void chop(int count)
    {
        assert(count <= size());
        Base::resize(size() - count);
    }

    int truncate(int count)
    {
        const int s = size();
        if (s > count) {
            Base::resize(count);
            return s - count;
        }
        return 0;
    }

    Set<T> toSet() const; // implemented in Set.h

    T &first()
    {
        return Base::operator[](0);
    }

    const T &first() const
    {
        return Base::at(0);
    }

    T takeFirst()
    {
        const T ret = first();
        removeFirst();
        return ret;
    }

    T &last()
    {
        return Base::operator[](size() - 1);
    }

    T takeLast()
    {
        const T ret = last();
        removeLast();
        return ret;
    }

    const T &last() const
    {
        return Base::at(size() - 1);
    }

    bool startsWith(const List<T> &t) const
    {
        if (size() < t.size())
            return false;
        for (int i = 0; i < t.size(); ++i) {
            if (Base::at(i) != t.Base::at(i))
                return false;
        }
        return true;
    }

    List<T> operator+(const T &t) const
    {
        const int s = Base::size();
        List<T> ret(s + 1);
        for (int i=0; i<s; ++i)
            ret[i] = Base::at(i);
        ret[s] = t;
        return ret;
    }

    List<T> operator+(const List<T> &t) const
    {
        if (t.isEmpty())
            return *this;

        int s = Base::size();
        List<T> ret(s + t.size());

        for (int i=0; i<s; ++i)
            ret[i] = Base::at(i);

        for (typename List<T>::const_iterator it = t.begin(); it != t.end(); ++it)
            ret[s++] = *it;

        return ret;
    }

    template <typename K>
    int compare(const List<K> &other) const
    {
        const int me = size();
        const int him = other.size();
        if (me < him) {
            return -1;
        } else if (me > him) {
            return 1;
        }
        typename List<K>::const_iterator bit = other.begin();
        for (typename List<T>::const_iterator it = Base::begin(); it != Base::end(); ++it) {
            const int cmp = it->compare(*bit);
            if (cmp)
                return cmp;
            ++bit;
        }
        return 0;
    }

    typename std::vector<T>::const_iterator constBegin() const
    {
        return std::vector<T>::begin();
    }

    typename std::vector<T>::const_iterator constEnd() const
    {
        return std::vector<T>::end();
    }

    template <typename K>
    bool operator==(const List<K> &other) const
    {
        return !compare(other);
    }

    template <typename K>
    bool operator!=(const List<K> &other) const
    {
        return compare(other);
    }

    template <typename K>
    bool operator<(const List<K> &other) const
    {
        return compare(other) < 0;
    }

    template <typename K>
    bool operator>(const List<K> &other) const
    {
        return compare(other) > 0;
    }

    List<T> &operator+=(const T &t)
    {
        append(t);
        return *this;
    }

    List<T> &operator+=(const List<T> &t)
    {
        append(t);
        return *this;
    }

    List<T> &operator<<(const T &t)
    {
        append(t);
        return *this;
    }

    List<T> &operator<<(const List<T> &t)
    {
        append(t);
        return *this;
    }
};

#endif
