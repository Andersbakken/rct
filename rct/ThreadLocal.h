#ifndef THREADLOCAL_H
#define THREADLOCAL_H

#include <pthread.h>

template<typename T>
class ThreadLocal
{
public:
    ThreadLocal() { init(); }
    ThreadLocal(const T& t) { init(); set(t); }
    ThreadLocal(T* t) { init(); set(t); }
    ~ThreadLocal() { clear(); }

    void clear() { pthread_key_delete(mKey); }

    void set(const T& t) { setData(new T(t)); }
    // takes ownership
    void set(T* t) { setData(t); }

    void remove() { setData(0); }
    bool has() const { return getData() != 0; }

    T* get() { return reinterpret_cast<T*>(getData()); }
    const T* get() const { return reinterpret_cast<const T*>(getData()); }

    T* operator->() { return reinterpret_cast<T*>(getData()); }
    const T* operator->() const { return reinterpret_cast<const T*>(getData()); }

    ThreadLocal<T>& operator=(const T& other) { set(other); return *this; }
    ThreadLocal<T>& operator=(const ThreadLocal<T>& other) { set(*other.get()); return *this; }

private:
    void init()
    {
        pthread_key_create(&mKey, deleteValue);
    }
    void setData(void* data)
    {
        delete reinterpret_cast<T*>(getData());
        pthread_setspecific(mKey, data);
    }
    void* getData() const
    {
        return pthread_getspecific(mKey);
    }
    static void deleteValue(void* val)
    {
        delete reinterpret_cast<T*>(val);
    }

private:
    pthread_key_t mKey;
};

#endif
