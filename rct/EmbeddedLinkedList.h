#ifndef EmbeddedLinkedList_h
#define EmbeddedLinkedList_h

template<typename T>
class EmbeddedLinkedList
{
public:
    EmbeddedLinkedList()
        : mFirst(T()), mLast(T()), mCount(0)
    {}

    bool isEmpty() const { return !mCount; }

    int size() const { return mCount; }
    int count() const { return mCount; }
    void insert(const T &t, T after = T()) // no reference here
    {
        assert(t);
        if (after) {
            t->next = after->next;
            if (t->next) {
                t->next->prev = t;
            } else {
                assert(mLast == after);
                mLast = t;
            }
            after->next = t;
            t->prev = after;
        } else if (!mFirst) {
            t->next = t->prev = T();
            mFirst = mLast = t;
        } else {
            t->next = mFirst;
            t->prev = T();
            mFirst->prev = t;
            mFirst = t;
        }
        ++mCount;
    }
    void append(const T &t) { insert(t, mLast); }
    void prepend(const T &t) { insert(t); }

    T &first() { return mFirst; }
    const T &first() const { return mFirst; }

    T &last() { return mLast; }
    const T &last() const { return mLast; }

    void remove(T t) // if we're removing a reference to a shared_ptr this could be problematic
    {
        assert(t);
        assert(t.use_count() > 1);
        if (t == mFirst) {
            if (t == mLast) {
                assert(mCount == 1);
                mFirst = mLast = T();
            } else {
                assert(mCount > 1);
                assert(t->next);
                mFirst = t->next;
                assert(mFirst);
                mFirst->prev = T();
            }
        } else if (t == mLast) {
            assert(mCount > 1);
            assert(t->prev);
            t->prev->next = T();
            mLast = t->prev;
        } else {
            assert(mCount > 1);
            assert(t->prev);
            assert(t->next);
            t->prev->next = t->next;
            t->next->prev = t->prev;
        }

        t->next = t->prev = T();
        assert(t);
        --mCount;
    }

    template <typename Type>
    struct iterator_base
    {
        iterator_base(const T &tt)
            : t(tt)
        {}

        Type operator++()
        {
            assert(t);
            t = t->next;
            return Type(t);
        }
        Type operator++(int)
        {
            Type ret(t);
            assert(t);
            t = t->next;
            return ret;
        }

        Type operator--()
        {
            assert(t);
            t = t->prev;
            return Type(t);
        }

        Type operator--(int)
        {
            Type ret(t);
            assert(t);
            t = t->prev;
            return ret;
        }

        T operator*() const { return t; }
        T operator->() const { return t; }

        bool operator==(const Type &other) { return t == other.t; }
        bool operator!=(const Type &other) { return t != other.t; }
    // private:
        T t;
    };

    struct iterator : public iterator_base<iterator>
    {
        iterator(const T &t)
            : iterator_base<iterator>(t)
        {}
    };
    struct const_iterator : public iterator_base<const_iterator>
    {
        const_iterator(const T &t)
            : iterator_base<const_iterator>(t)
        {}

    };

    void erase(const iterator &it)
    {
        assert(it.t);
        remove(it.t);
    }

    const_iterator begin() const { return const_iterator(mFirst); }
    const_iterator end() const { return const_iterator(T()); }

    iterator begin() { return iterator(mFirst); }
    iterator end() { return iterator(T()); }

    void removeFirst()
    {
        assert(mFirst);
        assert(mCount > 0);
        remove(mFirst);
    }

    void removeLast()
    {
        assert(mLast);
        remove(mLast);
    }

    T takeFirst()
    {
        assert(!isEmpty());
        const T t = first();
        removeFirst();
        return t;
    }
    T takeLast() { assert(!isEmpty()); const T t = last(); removeLast(); return t; }

    bool contains(const T &t) const
    {
        for (T tt = mFirst; tt; tt = tt->next) {
            if (t == tt)
                return true;
        }
        return false;
    }

    void deleteAll()
    {
        while (mCount) {
            T t = takeFirst();
            deleteNode(t);
        }
    }

    template <typename NodeType>
    void deleteNode(std::shared_ptr<NodeType> &) {}
    template <typename NodeType>
    void deleteNode(NodeType pointer)
    {
        delete pointer;
    }

    static void deletePointer(T &t)
    {
        delete t;
    }
// private:
    T mFirst, mLast;
    int mCount;
};

#endif
