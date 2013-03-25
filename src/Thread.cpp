#include "rct/Thread.h"

Thread::Thread(int stack)
    : mAutoDelete(false), mThread(0), mStackSize(stack)
{
}

Thread::~Thread()
{
}

void* Thread::internalStart(void* arg)
{
    Thread* that = reinterpret_cast<Thread*>(arg);
    that->run();
    if (that->isAutoDelete())
        delete that;
    return 0;
}

void Thread::start()
{
    pthread_attr_t attr;
    if (mStackSize) {
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, mStackSize);
        pthread_create(&mThread, &attr, internalStart, this);
        pthread_attr_destroy(&attr);
    } else {
        pthread_create(&mThread, 0, internalStart, this);
    }
}

bool Thread::join()
{
    void *ret;
    return !pthread_join(mThread, &ret);
}
