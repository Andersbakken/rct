#include "ReadWriteLock.h"

#include <assert.h>
#include <chrono>

ReadWriteLock::ReadWriteLock()
    : mCount(0), mWrite(false)
{
}

bool ReadWriteLock::lock(LockType type, int maxTime)
{
    std::unique_lock<std::mutex> locker(mMutex);
    bool ok;
    if (type == Read) {
        while (mWrite) {
            if (maxTime > 0) {
                ok = mCond.wait_for(locker, std::chrono::milliseconds(maxTime)) != std::cv_status::timeout;
            } else {
                mCond.wait(locker);
                ok = true;
            }
            if (!ok)
                return false;
        }
        ++mCount;
    } else {
        while (mCount) {
            if (maxTime > 0) {
                ok = mCond.wait_for(locker, std::chrono::milliseconds(maxTime)) != std::cv_status::timeout;
            } else {
                mCond.wait(locker);
                ok = true;
            }
            if (!ok)
                return false;
        }
        assert(!mWrite);
        mCount = 1;
        mWrite = true;
    }
    return true;
}

void ReadWriteLock::unlock()
{
    std::lock_guard<std::mutex> locker(mMutex);
    assert(mCount > 0);
    if (mCount > 1) {
        --mCount;
        assert(!mWrite);
        return;
    }
    --mCount;
    assert(!mCount);
    mWrite = false;
    mCond.notify_all();
}

bool ReadWriteLock::tryLock(LockType type)
{
    std::lock_guard<std::mutex> locker(mMutex);
    if (type == Read) {
        if (mWrite)
            return false;
        ++mCount;
        return true;
    } else {
        if (mCount > 0)
            return false;
        assert(!mWrite && !mCount);
        mCount = 1;
        mWrite = true;
        return true;
    }
}
