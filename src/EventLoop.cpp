#include "rct/EventLoop.h"
#include "rct/Rct.h"
#include "rct/Event.h"
#include "rct/EventReceiver.h"
#include "rct/MutexLocker.h"
#include "rct/ThreadLocal.h"
#include "rct/rct-config.h"
#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>

EventLoop* EventLoop::sInstance = 0;

EventLoop::EventLoop()
    : mQuit(false), mNextTimerHandle(0), mThread(0)
{
    if (!sInstance)
        sInstance = this;
    int flg;
    eintrwrap(flg, ::pipe(mEventPipe));
    eintrwrap(flg, ::fcntl(mEventPipe[0], F_GETFL, 0));
    eintrwrap(flg, ::fcntl(mEventPipe[0], F_SETFL, flg | O_NONBLOCK));
    eintrwrap(flg, ::fcntl(mEventPipe[1], F_GETFL, 0));
    eintrwrap(flg, ::fcntl(mEventPipe[1], F_SETFL, flg | O_NONBLOCK));
}

EventLoop::~EventLoop()
{
    int err;
    eintrwrap(err, ::close(mEventPipe[0]));
    eintrwrap(err, ::close(mEventPipe[1]));
}

EventLoop* EventLoop::instance()
{
    return sInstance;
}

bool EventLoop::timerLessThan(TimerData* a, TimerData* b)
{
    return !Rct::timevalGreaterEqualThan(&a->when, &b->when);
}

int EventLoop::addTimer(int timeout, TimerFunc callback, void* userData)
{
    MutexLocker locker(&mMutex);

    int handle = ++mNextTimerHandle;
    while (mTimerByHandle.find(handle) != mTimerByHandle.end()) {
        handle = ++mNextTimerHandle;
    }
    TimerData* data = new TimerData;
    data->handle = handle;
    data->timeout = timeout;
    data->callback = callback;
    data->userData = userData;
    Rct::gettime(&data->when);
    Rct::timevalAdd(&data->when, timeout);
    mTimerByHandle[handle] = data;

    List<TimerData*>::iterator it = std::lower_bound(mTimerData.begin(), mTimerData.end(),
                                                     data, timerLessThan);
    mTimerData.insert(it, data);

    const char c = 't';
    int r;
    while (true) {
        r = ::write(mEventPipe[1], &c, 1);
        if (r != -1 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            break;
        } else {
            sched_yield();
        }
    }

    return handle;
}

void EventLoop::removeTimer(int handle)
{
    MutexLocker locker(&mMutex);
    Map<int, TimerData*>::iterator it = mTimerByHandle.find(handle);
    if (it == mTimerByHandle.end())
        return;
    TimerData* data = it->second;
    mTimerByHandle.erase(it);
    List<TimerData*>::iterator dit = std::find(mTimerData.begin(), mTimerData.end(), data);
    assert(dit != mTimerData.end());
    assert((*dit)->handle == handle);
    mTimerData.erase(dit);
    delete data;
}

void EventLoop::addFileDescriptor(int fd, unsigned int flags, FdFunc callback, void* userData)
{
    MutexLocker locker(&mMutex);
    FdData &data = mFdData[fd];
    data.flags = flags;
    data.callback = callback;
    data.userData = userData;
    if (!pthread_equal(pthread_self(), mThread)) {
        const char c = 'f';
        int r;
        while (true) {
            r = ::write(mEventPipe[1], &c, 1);
            if (r != -1 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
                break;
            } else {
                sched_yield();
            }
        }
    }
}

void EventLoop::removeFileDescriptor(int fd, unsigned int flags)
{
    MutexLocker locker(&mMutex);
    if (!flags)
        mFdData.remove(fd);
    else {
        Map<int, FdData>::iterator it = mFdData.find(fd);
        if (it == mFdData.end())
            return;
        it->second.flags &= ~flags;
        if (!it->second.flags)
            mFdData.erase(it);
    }
}

void EventLoop::removeEvents(EventReceiver *receiver)
{
    MutexLocker locker(&mMutex);
    std::deque<EventData>::iterator it = mEvents.begin();
    while (it != mEvents.end()) {
        if (it->receiver == receiver) {
            delete it->event;
            it = mEvents.erase(it);
        } else {
            ++it;
        }
    }
}

void EventLoop::postEvent(EventReceiver* receiver, Event* event)
{
    {
        assert(receiver);
        EventData data = { receiver, event };

        MutexLocker locker(&mMutex);
        mEvents.push_back(data);
    }
    if (!pthread_equal(pthread_self(), mThread)) {
        const char c = 'e';
        int r;
        while (true) {
            r = ::write(mEventPipe[1], &c, 1);
            if (r != -1 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
                break;
            } else {
                sched_yield();
            }
        }
    }
}

void EventLoop::run()
{
    mQuit = false;
    mThread = pthread_self();
    fd_set rset, wset;
    int max;
    timeval timedata, timenow;
    for (;;) {
        FD_ZERO(&rset);
        FD_ZERO(&wset);
        FD_SET(mEventPipe[0], &rset);
        max = mEventPipe[0];
        for (Map<int, FdData>::const_iterator it = mFdData.begin();
             it != mFdData.end(); ++it) {
            if (it->second.flags & Read)
                FD_SET(it->first, &rset);
            if (it->second.flags & Write)
                FD_SET(it->first, &wset);
            max = std::max(max, it->first);
        }
        timeval* timeout;
        if (mTimerData.empty()) {
            timeout = 0;
        } else {
            Rct::gettime(&timenow);
            timedata = (*mTimerData.begin())->when;
            Rct::timevalSub(&timedata, &timenow);
            timeout = &timedata;
        }
        int r;
        // ### use poll instead? easier to catch exactly what fd that was problematic in the EBADF case
        eintrwrap(r, ::select(max + 1, &rset, &wset, 0, timeout));
        if (r == -1) { // ow
            error("Got error from select %d %s max %d used %d ", errno, strerror(errno), FD_SETSIZE, max + 1);
            return;
        }
        if (timeout) {
            Rct::gettime(&timenow);

            assert(mTimerData.begin() != mTimerData.end());
            List<TimerData> copy;
            {
                MutexLocker locker(&mMutex);
                List<TimerData*>::const_iterator it = mTimerData.begin();
                const List<TimerData*>::const_iterator end = mTimerData.end();
                while (it != end) {
                    copy.push_back(*(*it));
                    ++it;
                }
            }
            List<TimerData>::const_iterator it = copy.begin();
            const List<TimerData>::const_iterator end = copy.end();
            if (it != end) {
                while (true) {
                    if (!Rct::timevalGreaterEqualThan(&timenow, &it->when))
                        break;
                    if (reinsertTimer(it->handle, &timenow))
                        it->callback(it->handle, it->userData);
                    MutexLocker locker(&mMutex);
                    while (true) {
                        ++it;
                        if (it == end || mTimerByHandle.contains(it->handle))
                            break;
                    }

                    if (it == end)
                        break;
                }
            }
        }
        if (FD_ISSET(mEventPipe[0], &rset))
            handlePipe();
        Map<int, FdData> fds;
        {
            MutexLocker locker(&mMutex);
            fds = mFdData;
        }

        Map<int, FdData>::const_iterator it = fds.begin();
        while (it != fds.end()) {
            if ((it->second.flags & (Read|Disconnected)) && FD_ISSET(it->first, &rset)) {
                unsigned int flag = it->second.flags & Read;
                if (it->second.flags & Disconnected) {
                    size_t nbytes = 0;
                    int ret = ioctl(it->first, FIONREAD, reinterpret_cast<char*>(&nbytes));
                    if (!ret && !nbytes) {
                        flag |= Disconnected;
                    }
                }

                if ((it->second.flags & Write) && FD_ISSET(it->first, &wset))
                    flag |= Write;
                it->second.callback(it->first, flag, it->second.userData);
            } else if ((it->second.flags & Write) && FD_ISSET(it->first, &wset)) {
                it->second.callback(it->first, Write, it->second.userData);
            } else {
                ++it;
                continue;
            }
            MutexLocker locker(&mMutex);
            do {
                ++it;
            } while (it != fds.end() && !mFdData.contains(it->first));
        }
        if (mQuit)
            break;
    }
}

bool EventLoop::reinsertTimer(int handle, timeval* now)
{
    MutexLocker locker(&mMutex);

    // first, find the handle in the list and remove it
    List<TimerData*>::iterator it = mTimerData.begin();
    const List<TimerData*>::const_iterator end = mTimerData.end();
    while (it != end) {
        if ((*it)->handle == handle) {
            TimerData* data = *it;
            mTimerData.erase(it);
            // how much over the target time are we?
            const int overtime = Rct::timevalDiff(now, &data->when);
            data->when = *now;
            // the next time we want to fire is now + timeout - overtime
            // but we don't want a negative time
            Rct::timevalAdd(&data->when, std::max(data->timeout - overtime, 0));
            // insert the time so that the list stays sorted by absolute time
            it = std::lower_bound(mTimerData.begin(), mTimerData.end(),
                                  data, timerLessThan);
            mTimerData.insert(it, data);
            // all done
            return true;
        }
        ++it;
    }
    // didn't find our timer handle in the list
    return false;
}

void EventLoop::handlePipe()
{
    char c;
    for (;;) {
        int r;
        eintrwrap(r, ::read(mEventPipe[0], &c, 1));
        if (r == 1) {
            switch (c) {
            case 'e':
                sendPostedEvents();
                break;
            case 't':
            case 'f':
            case 'q':
                break;
            }
        } else
            break;
    }
}

void EventLoop::sendPostedEvents()
{
    MutexLocker locker(&mMutex);
    while (!mEvents.empty()) {
        std::deque<EventData>::iterator first = mEvents.begin();
        EventData data = *first;
        mEvents.erase(first);
        locker.unlock();
        data.receiver->sendEvent(data.event);
        delete data.event;
        locker.relock();
    }
}

void EventLoop::exit()
{
    MutexLocker lock(&mMutex);
    mQuit = true;
    if (!pthread_equal(pthread_self(), mThread)) {
        const char c = 'q';
        int r;
        while (true) {
            r = ::write(mEventPipe[1], &c, 1);
            if (r != -1 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
                break;
            } else {
                sched_yield();
            }
        }
    }
}
