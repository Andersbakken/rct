#include "rct/ThreadPool.h"
#include "rct/Thread.h"
#include "rct/MutexLocker.h"
#include <algorithm>
#include <assert.h>
#if defined (OS_FreeBSD) || defined (OS_NetBSD) || defined (OS_OpenBSD)
#   include <sys/types.h>
#   include <sys/sysctl.h>
#elif defined (OS_Linux)
#   include <unistd.h>
#elif defined (OS_Darwin)
#   include <sys/param.h>
#   include <sys/sysctl.h>
#endif

ThreadPool* ThreadPool::sInstance = 0;

class ThreadPoolThread : public Thread
{
public:
    ThreadPoolThread(ThreadPool* pool, int stackSize);
    ThreadPoolThread(const shared_ptr<ThreadPool::Job> &job, int stackSize);

    void stop();

protected:
    virtual void run();

private:
    shared_ptr<ThreadPool::Job> mJob;
    ThreadPool* mPool;
    bool mStopped;
};

ThreadPoolThread::ThreadPoolThread(ThreadPool* pool, int stackSize)
    : Thread(stackSize), mPool(pool), mStopped(false)
{
    setAutoDelete(false);
}

ThreadPoolThread::ThreadPoolThread(const shared_ptr<ThreadPool::Job> &job, int stackSize)
    : Thread(stackSize), mJob(job), mPool(0), mStopped(false)
{
    setAutoDelete(false);
}

void ThreadPoolThread::stop()
{
    MutexLocker locker(&mPool->mMutex);
    mStopped = true;
    mPool->mCond.wakeAll();
}

void ThreadPoolThread::run()
{
    if (mJob) {
        mJob->mMutex.lock();
        mJob->run();
        mJob->mMutex.unlock();
        return;
    }
    bool first = true;
    for (;;) {
        MutexLocker locker(&mPool->mMutex);
        if (!first) {
            --mPool->mBusyThreads;
        } else {
            first = false;
        }
        while (mPool->mJobs.empty() && !mStopped)
            mPool->mCond.wait(&mPool->mMutex);
        if (mStopped)
            break;
        std::deque<shared_ptr<ThreadPool::Job> >::iterator item = mPool->mJobs.begin();
        assert(item != mPool->mJobs.end());
        shared_ptr<ThreadPool::Job> job = *item;
        mPool->mJobs.erase(item);
        {
            MutexLocker lock(&job->mMutex);
            job->mState = ThreadPool::Job::Running;
        }
        ++mPool->mBusyThreads;
        locker.unlock();
        job->run();
        {
            MutexLocker lock(&job->mMutex);
            job->mState = ThreadPool::Job::Finished;
        }
    }
}

ThreadPool::ThreadPool(int concurrentJobs, int stackSize)
    : mConcurrentJobs(concurrentJobs), mStackSize(stackSize), mBusyThreads(0)
{
    if (!sInstance)
        sInstance = this;
    for (int i = 0; i < mConcurrentJobs; ++i) {
        mThreads.push_back(new ThreadPoolThread(this, mStackSize));
        mThreads.back()->start();
    }
}

ThreadPool::~ThreadPool()
{
    if (sInstance == this)
        sInstance = 0;
    MutexLocker locker(&mMutex);
    mJobs.clear();
    locker.unlock();
    for (List<ThreadPoolThread*>::iterator it = mThreads.begin();
         it != mThreads.end(); ++it) {
        ThreadPoolThread* t = *it;
        t->stop();
        t->join();
        delete t;
    }
}

void ThreadPool::setConcurrentJobs(int concurrentJobs)
{
    if (concurrentJobs == mConcurrentJobs)
        return;
    if (concurrentJobs > mConcurrentJobs) {
        MutexLocker locker(&mMutex);
        for (int i = mConcurrentJobs; i < concurrentJobs; ++i) {
            mThreads.push_back(new ThreadPoolThread(this, mStackSize));
            mThreads.back()->start();
        }
        mConcurrentJobs = concurrentJobs;
    } else {
        MutexLocker locker(&mMutex);
        for (int i = mConcurrentJobs; i > concurrentJobs; --i) {
            ThreadPoolThread* t = mThreads.back();
            mThreads.pop_back();
            locker.unlock();
            t->stop();
            t->join();
            locker.relock();
            delete t;
        }
        mConcurrentJobs = concurrentJobs;
    }
}

bool ThreadPool::jobLessThan(const shared_ptr<Job> &l, const shared_ptr<Job> &r)
{
    return static_cast<unsigned>(l->mPriority) > static_cast<unsigned>(r->mPriority);
}

void ThreadPool::start(const shared_ptr<Job> &job, int priority)
{
    job->mPriority = priority;
    if (priority == Guaranteed) {
        ThreadPoolThread *t = new ThreadPoolThread(job, mStackSize);
        t->start();
        return;
    }

    MutexLocker locker(&mMutex);
    if (mJobs.empty()) {
        mJobs.push_back(job);
    } else {
        if (mJobs.at(mJobs.size() - 1)->mPriority >= priority) {
            mJobs.push_back(job);
        } else if (mJobs.at(0)->mPriority < priority) {
            mJobs.push_front(job);
        } else {
            mJobs.push_back(job);
            std::sort(mJobs.begin(), mJobs.end(), jobLessThan);
        }
    }
    mCond.wakeOne();
}

bool ThreadPool::remove(const shared_ptr<Job> &job)
{
    MutexLocker locker(&mMutex);
    std::deque<shared_ptr<Job> >::iterator it = std::find(mJobs.begin(), mJobs.end(), job);
    if (it == mJobs.end())
        return false;
    mJobs.erase(it);
    return true;
}

int ThreadPool::idealThreadCount()
{
#if defined (OS_FreeBSD) || defined (OS_NetBSD) || defined (OS_OpenBSD)
    int cores;
    size_t len = sizeof(cores);
    int mib[2];
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    if (sysctl(mib, 2, &cores, &len, NULL, 0) != 0)
        return 1;
    return cores;
#elif defined (OS_Linux)
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
#elif defined (OS_Darwin)
    int cores;
    size_t len = sizeof(cores);
    int mib[2] = { CTL_HW, HW_AVAILCPU };
    if (sysctl(mib, 2, &cores, &len, NULL, 0)) {
        mib[1] = HW_NCPU;
        if (sysctl(mib, 2, &cores, &len, NULL, 0))
            return 1;
    }
    return cores;
#else
#   warning idealthreadcount not implemented on this platform
    return 1;
#endif
}

ThreadPool* ThreadPool::instance()
{
    if (!sInstance)
        sInstance = new ThreadPool(idealThreadCount());
    return sInstance;
}

ThreadPool::Job::Job()
    : mPriority(0), mState(NotStarted)
{
}

void ThreadPool::clearBackLog()
{
    MutexLocker locker(&mMutex);
    mJobs.clear();
}
