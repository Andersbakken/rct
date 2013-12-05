#ifndef THREAD_H
#define THREAD_H

#include <thread>
#include <mutex>

class Thread
{
public:
    Thread();
    virtual ~Thread();

    enum Priority { Idle, Normal };
    void start(Priority priority = Normal);
    bool join();

    void setAutoDelete(bool on)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mAutoDelete = on;
    }

    bool isAutoDelete() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mAutoDelete;
    }

    std::thread::id self() const { return mThread.get_id(); }

protected:
    virtual void run() = 0;

private:
    void finish();

private:
    bool mAutoDelete;
    mutable std::mutex mMutex;
    std::thread mThread;
};

#endif
