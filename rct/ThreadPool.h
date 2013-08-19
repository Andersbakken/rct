#ifndef THREADPOOL2_H
#define THREADPOOL2_H

#include "Apply.h"
#include "Thread.h"
#include <tuple>
#include <vector>
#include <map>
#include <mutex>
#include <condition_variable>

class ThreadPool
{
public:
    typedef unsigned int Key;

    ThreadPool(int concurrentJobs)
        : concurrent(concurrentJobs), nextId(0)
    {
        init();
    }
    ~ThreadPool()
    {
        clear();
    }

    template<typename Functor, typename... Args>
    Key start(Functor&& func, const Args&... args)
    {
        Job<Functor, Args...>* job = new Job<Functor, Args...>(std::move(func), args...);
        std::unique_lock<std::mutex> lock(mutex);
        const Key id = findNextId();
        jobs[id] = job;
        cond.notify_one();
        return id;
    }

    template<typename Functor, typename... Args>
    Key startMove(Functor&& func, Args&&... args)
    {
        Job<Functor, Args...>* job = new Job<Functor, Args...>(std::move(func), std::move(args...));
        std::unique_lock<std::mutex> lock(mutex);
        const Key id = findNextId();
        jobs[id] = job;
        cond.notify_one();
        return id;
    }

    bool remove(Key key)
    {
        std::unique_lock<std::mutex> lock(mutex);
        std::map<Key, JobBase*>::iterator it = jobs.find(key);
        if (it != jobs.end()) {
            jobs.erase(it);
            return true;
        }
        return false;
    }

    int concurrentJobs() const { std::lock_guard<std::mutex> lock(mutex); return concurrent; }
    void setConcurrentJobs(int concurrentJobs);

private:
    struct JobBase
    {
        virtual ~JobBase() { }
        virtual void exec() = 0;
    };

    template<typename Functor, typename... Args>
    struct Job : public JobBase
    {
        Functor functor;
        std::tuple<Args...> args;

        virtual void exec() { applyMove(functor, args); }
    };

    Key findNextId()
    {
        while (jobs.find(++nextId) != jobs.end())
            ;
        return nextId;
    }

    class Thread : public ::Thread
    {
    public:
        Thread(ThreadPool* threadPool)
            : pool(threadPool)
        {
        }

    protected:
        virtual void run();

    private:
        ThreadPool* pool;
    };

    void init();
    void clear();
    void stop();
    JobBase* nextJob();

private:
    int concurrent;
    Key nextId;
    bool stopped;
    std::map<Key, JobBase*> jobs;
    std::vector<ThreadPool::Thread> threads;
    std::condition_variable cond;
    mutable std::mutex mutex;

    friend class ThreadPool::Thread;
};

#endif
