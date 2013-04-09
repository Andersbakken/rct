#ifndef MUTEXLOCKER_H
#define MUTEXLOCKER_H

#include <rct/Mutex.h>

class MutexLocker
{
public:
    MutexLocker(Mutex* mutex = 0)
        : mMutex(mutex), mLocked(mutex != 0)
    {
        if (mMutex)
            mMutex->lock();
    }
    ~MutexLocker() { if (mLocked) mMutex->unlock(); }

    void lock(Mutex* mutex) { if (mLocked) unlock(); mMutex = mutex; mMutex->lock(); mLocked = true; }

    bool isLocked() const { return mLocked; }
    void unlock() { if (mLocked) { mMutex->unlock(); mLocked = false; } }
    void relock() { if (!mLocked) { mMutex->lock(); mLocked = true; } }

private:
    Mutex* mMutex;
    bool mLocked;
};

#endif
