#ifndef EmbeddedLinkedList_h
#define EmbeddedLinkedList_h

#include <assert.h>

template <typename T>
class EmbeddedLinkedList
{
public:
    EmbeddedLinkedList()
        : mFront(T())
        , mBack(T())
        , mCount(0)
    {
    }

    ~EmbeddedLinkedList()
    {
        deleteAll();
    }

    bool isEmpty() const
    {
        return !mCount;
    }

    bool empty() const
    {
        return !mCount;
    }

    size_t size() const
    {
        return mCount;
    }

    size_t count() const
    {
        return mCount;
    }

    void insert(const T &t, T after = T()) // no reference here
    {
        assert(t);
        if (after) {
            t->next = after->next;
            if (t->next) {
                t->next->prev = t;
            } else {
                assert(mBack == after);
                mBack = t;
            }
            after->next = t;
            t->prev     = after;
        } else if (!mFront) {
            t->next = t->prev = T();
            mFront = mBack = t;
        } else {
            t->next      = mFront;
            t->prev      = T();
            mFront->prev = t;
            mFront       = t;
        }
        ++mCount;
    }

    void append(const T &t)
    {
        insert(t, mBack);
    }

    void prepend(const T &t)
    {
        insert(t);
    }

    void push_back(const T &t)
    {
        insert(t, mBack);
    }

    void push_front(const T &t)
    {
        insert(t);
    }

    T &first()
    {
        return mFront;
    }

    const T &first() const
    {
        return mFront;
    }

    T &front()
    {
        return mFront;
    }

    const T &front() const
    {
        return mFront;
    }

    T &last()
    {
        return mBack;
    }

    const T &last() const
    {
        return mBack;
    }

    T &back()
    {
        return mBack;
    }

    const T &back() const
    {
        return mBack;
    }

    void remove(const T &tt)
    {
        T t = tt; // prevent shared_ptr from disappearing underneath us
        assert(t);
        if (t == mFront) {
            if (t == mBack) {
                assert(mCount == 1);
                mFront = mBack = T();
            } else {
                assert(mCount > 1);
                assert(t->next);
                mFront = t->next;
                assert(mFront);
                mFront->prev = T();
            }
        } else if (t == mBack) {
            assert(mCount > 1);
            assert(t->prev);
            t->prev->next = T();
            mBack         = t->prev;
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
        {
        }

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

        T operator*() const
        {
            return t;
        }

        T operator->() const
        {
            return t;
        }

        bool operator==(const Type &other)
        {
            return t == other.t;
        }

        bool operator!=(const Type &other)
        {
            return t != other.t;
        }

    private:
        T t;
    };

    struct iterator : public iterator_base<iterator>
    {
        iterator(const T &val)
            : iterator_base<iterator>(val)
        {
        }
    };

    struct const_iterator : public iterator_base<const_iterator>
    {
        const_iterator(const T &val)
            : iterator_base<const_iterator>(val)
        {
        }
    };

    void erase(const iterator &it)
    {
        assert(it.t);
        remove(it.t);
    }

    const_iterator begin() const
    {
        return const_iterator(mFront);
    }

    const_iterator end() const
    {
        return const_iterator(T());
    }

    iterator begin()
    {
        return iterator(mFront);
    }

    iterator end()
    {
        return iterator(T());
    }

    T removeFirst()
    {
        return removeFront();
    }

    T removeLast()
    {
        return removeBack();
    }

    T removeFront()
    {
        assert(mFront);
        assert(mCount > 0);
        const T copy = mFront;
        remove(copy);
        return copy;
    }

    T removeBack()
    {
        assert(mBack);
        const T copy = mBack;
        remove(copy);
        return copy;
    }

    T takeFront()
    {
        return removeFirst();
    }

    T takeBack()
    {
        return removeLast();
    }

    T takeFirst()
    {
        return removeFirst();
    }

    T takeLast()
    {
        return removeLast();
    }

    bool contains(const T &t) const
    {
        for (T tt = mFront; tt; tt = tt->next) {
            if (t == tt)
                return true;
        }
        return false;
    }

    void deleteAll()
    {
        T t = mFront;
        while (t) {
            T next  = t->next;
            t->next = t->prev = T();
            deleteNode(t);
            t = next;
        }
        mFront = mBack = nullptr;
        mCount         = 0;
    }

    void moveToEnd(const T &t)
    {
        assert(mCount);
        if (t->next) {
            T copy = t;
            remove(t);
            append(copy);
        }
    }

    void moveToFront(const T &t)
    {
        assert(mCount);
        if (t->prev) {
            T copy = t;
            remove(t);
            prepend(copy);
        }
    }

private:
    template <typename NodeType>
    void deleteNode(std::shared_ptr<NodeType> &)
    {
    }

    template <typename NodeType>
    void deleteNode(NodeType pointer)
    {
        delete pointer;
    }

    static void deletePointer(T &t)
    {
        delete t;
    }

    T mFront, mBack;
    size_t mCount;
};

#endif
