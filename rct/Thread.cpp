#include "Thread.h"
#include "EventLoop.h"
#include "Log.h"
#include "rct-config.h"

Thread::Thread()
    : mAutoDelete(false)
{
}

Thread::~Thread()
{
}

void Thread::start(Priority priority)
{
    mThread = std::thread([=]() {
            run();
            if (isAutoDelete())
                EventLoop::mainEventLoop()->callLater(std::bind(&Thread::finish, this));
        });
    if (priority == Idle) {
#ifdef HAVE_SCHEDIDLE
        sched_param param = { 0 };
        if (pthread_setschedparam(mThread.native_handle(), SCHED_IDLE, &param) == -1) {
            error() << "pthread_setschedparam failed";
        }
#endif
    }
}

bool Thread::join()
{
    mThread.join();
    return true;
}

void Thread::finish()
{
    join();
    delete this;
}
