#ifndef ThreadPool_h
#define ThreadPool_h

#include <rct/Mutex.h>
#include <rct/WaitCondition.h>
#include <rct/Memory.h>
#include <deque>

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
        virtual ~Job();

    protected:
        virtual void run() {}
    private:
        int mPriority;
        Mutex mMutex;

        friend class ThreadPool;
        friend class ThreadPoolThread;
    };

    enum { Guaranteed = -1 };

    void start(const shared_ptr<Job> &job, int priority = 0);

    static int idealThreadCount();
    static ThreadPool* globalInstance();

private:
    static bool jobLessThan(const shared_ptr<Job> &l, const shared_ptr<Job> &r);

private:
    int mConcurrentJobs;
    const int mStackSize;
    Mutex mMutex;
    WaitCondition mCond;
    std::deque<shared_ptr<Job> > mJobs;
    List<ThreadPoolThread*> mThreads;
    int mBusyThreads;

    static ThreadPool* sGlobalInstance;

    friend class ThreadPoolThread;
};

#endif
