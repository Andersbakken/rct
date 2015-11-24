#include "Thread.h"

#include <rct/EventLoop.h>
#include <rct/Log.h>
#include <rct/rct-config.h>

Thread::Thread()
    : mAutoDelete(false), mRunning(false), mLoop(EventLoop::eventLoop())
{
}

Thread::~Thread()
{
    if (mRunning)
        pthread_cancel(mThread);
}

void* Thread::localStart(void* arg)
{
    Thread* t = static_cast<Thread*>(arg);
    t->run();
    EventLoop::cleanupLocalEventLoop();
    if (t->isAutoDelete()) {
        if (EventLoop::SharedPtr loop = t->mLoop.lock())
            loop->callLater(std::bind(&Thread::finish, t));
        else
            EventLoop::mainEventLoop()->callLater(std::bind(&Thread::finish, t));
    }
    return 0;
}

static inline void initAttr(pthread_attr_t** pattr, pthread_attr_t* attr)
{
    if (!*pattr) {
        *pattr = attr;
        pthread_attr_init(attr);
    }
}

void Thread::start(Priority priority, size_t stackSize)
{
    pthread_attr_t attr;
    pthread_attr_t* pattr = 0;
    if (priority == Idle) {
#ifdef HAVE_SCHEDIDLE
        initAttr(&pattr, &attr);
        if (pthread_attr_setschedpolicy(pattr, SCHED_IDLE) != 0) {
            error() << "pthread_attr_setschedpolicy failed";
        }
#endif
    }
    if (stackSize > 0) {
        initAttr(&pattr, &attr);
        if (pthread_attr_setstacksize(pattr, stackSize) != 0) {
            error() << "pthread_attr_setstacksize failed";
        }
    }
    mRunning = true;
    if (pthread_create(&mThread, pattr, localStart, this) != 0) {
        error() << "pthread_create failed";
    }
    if (pattr) {
        pthread_attr_destroy(pattr);
    }
}

bool Thread::join()
{
    if (!mRunning)
        return false;
    const bool ok = pthread_join(mThread, 0) == 0;
    mRunning = false;
    return ok;
}

void Thread::finish()
{
    join();
    delete this;
}
