#ifndef ThreadPool_h
#define ThreadPool_h

#include <rct/List.h>
#include <rct/Thread.h>
#include <stddef.h>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <functional>

#include "rct/List.h"
#include "rct/Thread.h"

class ThreadPoolThread;

class ThreadPool
{
public:
    ThreadPool(int concurrentJobs,
               Thread::Priority priority = Thread::Normal,
               size_t stackSize = 0);
    ~ThreadPool();

    void setConcurrentJobs(int concurrentJobs);
    void clearBackLog();
    int backlogSize() const;

    class Job
    {
    public:
        Job();
        virtual ~Job() {}

        enum State {
            NotStarted,
            Running,
            Finished
        };
        State state() const
        {
            std::unique_lock<std::mutex> lock(mMutex);
            return mState;
        }
        void waitForState(State state)
        {
            std::unique_lock<std::mutex> lock(mMutex);
            while (mState != state)
                mCond.wait(lock);
        }
    protected:
        virtual void run() = 0;
        std::mutex &mutex() const { return mMutex; }

    private:
        int mPriority;
        State mState;
        mutable std::mutex mMutex;
        std::condition_variable mCond;

        friend class ThreadPool;
        friend class ThreadPoolThread;
    };

    enum { Guaranteed = -1 };

    void start(const std::shared_ptr<Job> &job, int priority = 0);
    void start(const std::function<void()> &func, int priority = 0);

    bool remove(const std::shared_ptr<Job> &job);

    static int idealThreadCount();
    static ThreadPool* instance();

    int busyThreads() const;
private:
    static bool jobLessThan(const std::shared_ptr<Job> &l, const std::shared_ptr<Job> &r);

private:
    int mConcurrentJobs;
    mutable std::mutex mMutex;
    std::condition_variable mCond;
    std::deque<std::shared_ptr<Job> > mJobs;
    List<ThreadPoolThread*> mThreads;
    int mBusyThreads;
    const Thread::Priority mPriority;
    const size_t mThreadStackSize;

    static ThreadPool* sInstance;

    friend class ThreadPoolThread;
};

#endif
