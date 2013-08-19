#include "ThreadPool.h"
#include <assert.h>

void ThreadPool::init()
{
    // start the threads
    for (int i = 0; i < concurrent; ++i) {
        ThreadPool::Thread thr(this);
        thr.start();
        threads.push_back(std::move(thr));
    }
}

void ThreadPool::clear()
{
    // stop the threads
    stop();

    // now join
    auto thread = threads.begin();
    const auto end = threads.end();
    while (thread != end) {
        thread->join();
        ++thread;
    }
    threads.clear();
}

void ThreadPool::stop()
{
    std::unique_lock<std::mutex> lock(mutex);
    stopped = true;
    cond.notify_all();
}

void ThreadPool::setConcurrentJobs(int concurrentJobs)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        concurrent = concurrentJobs;
    }
    clear();
    init();
}

ThreadPool::JobBase* ThreadPool::nextJob()
{
    std::unique_lock<std::mutex> lock(mutex);
    for (;;) {
        if (stopped)
            return 0;
        std::map<Key, JobBase*>::iterator it = jobs.begin();
        if (it != jobs.end()) {
            JobBase* job = it->second;
            jobs.erase(it);
            return job;
        }
        cond.wait(lock);
    }
}

void ThreadPool::Thread::run()
{
    for (;;) {
        ThreadPool::JobBase* job = pool->nextJob();
        {
            std::lock_guard<std::mutex>(pool->mutex);
            if (pool->stopped) {
                delete job;
                break;
            }
        }
        if (!job)
            continue;
        job->exec();
        delete job;
    }
}
