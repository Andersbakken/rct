#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>
#include <rct/EventLoop.h>
#include <stddef.h>
#include <mutex>
#include <memory>

class EventLoop;

class Thread
{
public:
    Thread();
    virtual ~Thread();

    enum Priority { Idle, Normal };
    bool start(Priority priority = Normal, size_t stackSize = 0);
    bool join();

    void setAutoDelete(bool on)
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mAutoDelete = on;
    }

    bool isAutoDelete() const
    {
        std::unique_lock<std::mutex> lock(mMutex);
        return mAutoDelete;
    }

    pthread_t self() const
    {
        std::unique_lock<std::mutex> lock(mMutex);
        return mThread;
    }

protected:
    virtual void run() = 0;

private:
    void finish();

    static void* localStart(void* arg);

private:
    bool mAutoDelete;
    mutable std::mutex mMutex;
    pthread_t mThread;
    bool mRunning;
    std::weak_ptr<EventLoop> mLoop;
};

#endif
