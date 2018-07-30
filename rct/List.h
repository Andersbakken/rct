#ifndef List_h
#define List_h

#include <assert.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <vector>
#include <string>

template <typename T> class Set;

template <typename T>
class List : public std::vector<T>
{
    typedef std::vector<T> Base;
public:
    static const size_t npos = std::string::npos;
    explicit List(size_t count, T &&defaultValue = T())
        : Base(count, std::move(defaultValue))
    {}

    explicit List()
        : Base()
    {}

    template <typename CompatibleType>
    List(const std::vector<CompatibleType> &other)
        : Base(other.size(), T())
    {
        const size_t len = other.size();
        for (size_t i=0; i<len; ++i) {
            std::vector<T>::operator[](i) = other.at(i);
        }
    }

    template <typename CompatibleType>
    List(std::vector<CompatibleType> &&other)
        : Base(other.size(), T())
    {
        const size_t len = other.size();
        for (size_t i=0; i<len; ++i) {
            std::vector<T>::operator[](i) = std::move(other.at(i));
        }
    }

    List(std::initializer_list<T> list)
        : Base(list)
    {}

    List(typename Base::const_iterator f, typename Base::const_iterator l)
        : Base(f, l)
    {
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

    void append(T &&t)
    {
        Base::push_back(std::forward<T>(t));
    }

    void prepend(T &&t)
    {
        Base::insert(Base::begin(), std::forward<T>(t));
    }

    void append(const List<T> &t)
    {
        const size_t len = t.size();
        for (size_t i=0; i<len; ++i)
            Base::push_back(t.at(i));
    }

    void insert(size_t idx, const List<T> &list)
    {
        Base::insert(Base::begin() + idx, list.begin(), list.end());
    }

    void insert(size_t idx, const T &val)
    {
        Base::insert(Base::begin() + idx, val);
    }

    void sort()
    {
        std::sort(Base::begin(), Base::end());
    }

    void sort(std::function<bool(const T &, const T &r)> func)
    {
        std::sort(Base::begin(), Base::end(), func);
    }

    size_t indexOf(const T &t) const
    {
        const typename Base::const_iterator beg = Base::begin();
        const typename Base::const_iterator end = Base::end();
        const typename Base::const_iterator it = std::find(beg, end, t);
        return it == end ? npos : (it - beg);
    }

    size_t lastIndexOf(const T &t, int from = -1) const
    {
        const size_t s = size();
        if (!s)
            return npos;
        if (from < 0) {
            from += s;
        }
        from = std::min<int>(s - 1, from);
        if (from >= 0) {
            const T *haystack = Base::constData();
            const T *needle = haystack + from + 1;
            while (needle != haystack) {
                if (*--needle == t)
                    return needle - haystack;
            }
        }
        return npos;
    }

    size_t remove(const T &t)
    {
        size_t ret = 0;
        while (true) {
            typename Base::iterator it = std::find(Base::begin(), Base::end(), t);
            if (it == Base::end())
                break;
            Base::erase(it);
            ++ret;
        }
        return ret;
    }

    void removeAt(size_t idx)
    {
        Base::erase(Base::begin() + idx);
    }

    void remove(size_t idx, size_t count)
    {
        Base::erase(Base::begin() + idx, Base::begin() + idx + count);
    }

    void removeLast()
    {
        Base::pop_back();
    }

    void removeFirst()
    {
        Base::erase(Base::begin());
    }

    size_t size() const
    {
        return Base::size();
    }

    T value(size_t idx, const T &defaultValue) const
    {
        return idx < Base::size() ? Base::at(idx) : defaultValue;
    }

    T value(size_t idx) const
    {
        return idx < Base::size() ? Base::at(idx) : T();
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

    void deleteAll(std::function<void(T)> deleter)
    {
        typename Base::iterator it = Base::begin();
        while (it != Base::end()) {
            deleter(*it);
            ++it;
        }
        Base::clear();
    }


    void chop(size_t count)
    {
        assert(count <= size());
        Base::resize(size() - count);
    }

    size_t truncate(size_t count)
    {
        const size_t s = size();
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

    List<T> mid(size_t from, int len = -1) const
    {
        assert(from >= 0);
        const size_t count = Base::size();
        if (from >= count)
            return List<T>();
        if (len < 0) {
            len = count - from;
        } else {
            len = std::min<int>(count - from, len);
        }
        return List<T>(Base::begin() + from, Base::begin() + from + len);
    }

    bool startsWith(const List<T> &t) const
    {
        if (size() < t.size())
            return false;
        for (size_t i = 0; i < t.size(); ++i) {
            if (Base::at(i) != t.Base::at(i))
                return false;
        }
        return true;
    }

    List<T> operator+(const T &t) const
    {
        const size_t s = Base::size();
        List<T> ret(s + 1);
        for (size_t i=0; i<s; ++i)
            ret[i] = Base::at(i);
        ret[s] = t;
        return ret;
    }

    List<T> operator+(const List<T> &t) const
    {
        if (t.isEmpty())
            return *this;

        size_t s = Base::size();
        List<T> ret(s + t.size());

        for (size_t i=0; i<s; ++i)
            ret[i] = Base::at(i);

        for (typename List<T>::const_iterator it = t.begin(); it != t.end(); ++it)
            ret[s++] = *it;

        return ret;
    }

    template <typename K>
    int compare(const List<K> &other) const
    {
        const size_t me = size();
        const size_t him = other.size();
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

    size_t remove(std::function<bool(const T &t)> match)
    {
        size_t ret = 0;
        typename Base::iterator it = Base::begin();
        while (it != Base::end()) {
            if (match(*it)) {
                it = Base::erase(it);
                ++ret;
            } else {
                ++it;
            }
        }
        return ret;
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
