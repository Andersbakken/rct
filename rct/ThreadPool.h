#ifndef ThreadPool_h
#define ThreadPool_h

#include "List.h"
#include <deque>
#include <memory>
#include <mutex>
#include <condition_variable>

class ThreadPoolThread;

class ThreadPool
{
public:
    ThreadPool(int concurrentJobs, int stackSize = 0);
    ~ThreadPool();

    void setConcurrentJobs(int concurrentJobs);
    void clearBackLog();

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
        State state() const { std::lock_guard<std::mutex> lock(mMutex); return mState; }
    protected:
        virtual void run() = 0;
        std::mutex &mutex() const { return mMutex; }

    private:
        int mPriority;
        State mState;
        mutable std::mutex mMutex;

        friend class ThreadPool;
        friend class ThreadPoolThread;
    };

    enum { Guaranteed = -1 };

    void start(const std::shared_ptr<Job> &job, int priority = 0);

    bool remove(const std::shared_ptr<Job> &job);

    static int idealThreadCount();
    static ThreadPool* instance();

private:
    static bool jobLessThan(const std::shared_ptr<Job> &l, const std::shared_ptr<Job> &r);

private:
    int mConcurrentJobs;
    const int mStackSize;
    std::mutex mMutex;
    std::condition_variable mCond;
    std::deque<std::shared_ptr<Job> > mJobs;
    List<ThreadPoolThread*> mThreads;
    int mBusyThreads;

    static ThreadPool* sInstance;

    friend class ThreadPoolThread;
};

#endif
