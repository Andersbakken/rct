#ifndef READWRITELOCK_H
#define READWRITELOCK_H

#include <mutex>
#include <condition_variable>

class ReadWriteLock
{
public:
    ReadWriteLock();

    enum LockType {
        Read,
        Write
    };

    bool lockForRead(int maxTime = 0) { return lock(Read, maxTime); }
    bool lockForWrite(int maxTime = 0) { return lock(Write, maxTime); }
    bool lock(LockType type, int maxTime = 0);

    bool tryLockForRead() { return tryLock(Read); }
    bool tryLockForWrite() { return tryLock(Write); }
    bool tryLock(LockType type);

    void unlock();

private:
    std::mutex mMutex;
    std::condition_variable mCond;
    int mCount;
    bool mWrite;
};

#endif
