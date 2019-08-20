#ifndef READLOCKER_H
#define READLOCKER_H

#include <rct/ReadWriteLock.h>

class ReadLocker
{
public:
    ReadLocker(ReadWriteLock* lock)
        : mLock(lock)
    {
        if (mLock && !mLock->lockForRead())
            mLock = nullptr;
    }
    ~ReadLocker()
    {
        if (mLock)
            mLock->unlock();
    }

private:
    ReadWriteLock* mLock;
};

#endif
