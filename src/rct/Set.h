#ifndef Set_h
#define Set_h

#include <cpp-btree/safe_btree_set.h>
#include <rct/List.h>

template <typename T>
class Set : public btree::safe_btree_set<T>
{
public:
    Set() {}

    bool contains(const T &t) const
    {
        return btree::safe_btree_set<T>::find(t) != btree::safe_btree_set<T>::end();
    }

    bool isEmpty() const
    {
        return !btree::safe_btree_set<T>::size();
    }

    bool remove(const T &t)
    {
        typename btree::safe_btree_set<T>::iterator it = btree::safe_btree_set<T>::find(t);
        if (it != btree::safe_btree_set<T>::end()) {
            btree::safe_btree_set<T>::erase(it);
            return true;
        }
        return false;
    }
    List<T> toList() const
    {
        List<T> ret(size());
        typename btree::safe_btree_set<T>::iterator it = btree::safe_btree_set<T>::begin();
        int i = 0;
        while (it != btree::safe_btree_set<T>::end()) {
            ret[i++] = *it;
            ++it;
        }
        return ret;
    }

    bool insert(const T &t)
    {
        return btree::safe_btree_set<T>::insert(t).second;
    }

    Set<T> &unite(const Set<T> &other, int *count = 0)
    {
        int c = 0;
        if (isEmpty()) {
            *this = other;
            c = other.size();
        } else {
            typename btree::safe_btree_set<T>::iterator it = other.begin();
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

    Set<T> &unite(const List<T> &other, int *count = 0)
    {
        int c = 0;
        typename std::vector<T>::iterator it = other.begin();
        while (it != other.end()) {
            if (insert(*it))
                ++c;
            ++it;
        }
        if (count)
            *count = c;
        return *this;
    }

    Set<T> &subtract(const Set<T> &other, int *count = 0)
    {
        int c = 0;
        if (!isEmpty()) {
            typename btree::safe_btree_set<T>::iterator it = other.begin();
            while (it != other.end()) {
                c += btree::safe_btree_set<T>::erase(*it);
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

    int size() const
    {
        return btree::safe_btree_set<T>::size();
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
    const int s = size();
    for (int i=0; i<s; ++i) {
        ret.insert(std::vector<T>::at(i));
    }
    return ret;
}


#endif
