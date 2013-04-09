#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>
#include <rct/Mutex.h>
#include <rct/MutexLocker.h>

class Thread
{
public:
    Thread(int stackSize = 0);
    virtual ~Thread();

    void start();
    bool join();

    void setAutoDelete(bool on)
    {
        MutexLocker lock(&mMutex);
        mAutoDelete = on;
    }
    bool isAutoDelete() const
    {
        MutexLocker lock(&mMutex);
        return mAutoDelete;
    }
    pthread_t self() const { return mThread; }
protected:
    virtual void run() = 0;

private:
    static void* internalStart(void* arg);

private:
    bool mAutoDelete;
    mutable Mutex mMutex;
    pthread_t mThread;
    const int mStackSize;
};

#endif
