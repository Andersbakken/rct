#include "EventLoop.h"
#include "Timer.h"
#include "rct-config.h"
#include <algorithm>
#include <atomic>
#include <set>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#if defined(HAVE_EPOLL)
#  include <sys/epoll.h>
#elif defined(HAVE_KQUEUE)
#  include <sys/event.h>
#  include <sys/time.h>
#endif
#ifdef HAVE_MACH_ABSOLUTE_TIME
#  include <mach/mach.h>
#  include <mach/mach_time.h>
#endif

// this is pretty awful, any better ideas to avoid the unused warning?
#define STRERROR_R(errno, buf, size) if (strerror_r(errno, buf, size))

EventLoop::WeakPtr EventLoop::mainLoop;
std::mutex EventLoop::mainMutex;
static std::atomic<int> mainEventPipe;
static std::once_flag mainOnce;
static pthread_key_t eventLoopKey;

// sadly GCC < 4.8 doesn't support thread_local
// fall back to pthread instead in order to support 4.7

// ### static leak, one EventLoop::WeakPtr for each thread
// ### that calls EventLoop::eventLoop()
static EventLoop::WeakPtr& localEventLoop()
{
    EventLoop::WeakPtr* ptr = static_cast<EventLoop::WeakPtr*>(pthread_getspecific(eventLoopKey));
    if (!ptr) {
        ptr = new EventLoop::WeakPtr;
        pthread_setspecific(eventLoopKey, ptr);
    }
    return *ptr;
}

#define eintrwrap(VAR, BLOCK)                   \
    do {                                        \
        VAR = BLOCK;                            \
    } while (VAR == -1 && errno == EINTR)

static void signalHandler(int sig, siginfo_t *siginfo, void *context)
{
    char b = 'q';
    int w;
    const int pipe = mainEventPipe;
    if (pipe != -1)
        eintrwrap(w, ::write(pipe, &b, 1));
}

EventLoop::EventLoop()
    : pollFd(-1), nextTimerId(0), stop(false), exitCode(0), flgs(0)
{
    std::call_once(mainOnce, [](){
            mainEventPipe = -1;
            pthread_key_create(&eventLoopKey, 0);
            signal(SIGPIPE, SIG_IGN);
        });
}

EventLoop::~EventLoop()
{
    assert(mExecStack.empty());
    cleanup();
}

void EventLoop::init(unsigned flags)
{
    std::lock_guard<std::mutex> locker(mutex);
    flgs = flags;

    threadId = std::this_thread::get_id();
    int e = ::pipe(eventPipe);
    if (e == -1) {
        eventPipe[0] = -1;
        eventPipe[1] = -1;
        cleanup();
        return;
    }
    eintrwrap(e, ::fcntl(eventPipe[0], F_GETFL, 0));
    if (e == -1) {
        cleanup();
        return;
    }

    eintrwrap(e, ::fcntl(eventPipe[0], F_SETFL, e | O_NONBLOCK));
    if (e == -1) {
        cleanup();
        return;
    }

#if defined(HAVE_EPOLL)
    pollFd = epoll_create1(0);
#elif defined(HAVE_KQUEUE)
    pollFd = kqueue();
#else
#error No supported event polling mechanism
#endif
    if (pollFd == -1) {
        cleanup();
        return;
    }

#if defined(HAVE_EPOLL)
    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = eventPipe[0];
    e = epoll_ctl(pollFd, EPOLL_CTL_ADD, eventPipe[0], &ev);
#elif defined(HAVE_KQUEUE)
    struct kevent ev;
    memset(&ev, '\0', sizeof(struct kevent));
    ev.ident = eventPipe[0];
    ev.flags = EV_ADD|EV_ENABLE;
    ev.filter = EVFILT_READ;
    eintrwrap(e, kevent(pollFd, &ev, 1, 0, 0, 0));
#endif
    if (e == -1) {
        cleanup();
        return;
    }

    if (flgs & EnableSigIntHandler) {
        struct sigaction act;
        act.sa_sigaction = signalHandler;
        act.sa_flags = SA_SIGINFO;
        if (::sigaction(SIGINT, &act, 0) == -1) {
            cleanup();
            return;
        }
    }

    std::shared_ptr<EventLoop> that = shared_from_this();
    localEventLoop() = that;
    if (flags & MainEventLoop)
        mainLoop = that;
}

void EventLoop::cleanup()
{
    std::lock_guard<std::mutex> locker(mutex);
    localEventLoop().reset();

    while (!events.empty()) {
        delete events.front();
        events.pop();
    }

    if (pollFd != -1)
        ::close(pollFd);

    if (eventPipe[0] != -1)
        ::close(eventPipe[0]);
    if (eventPipe[1] != -1)
        ::close(eventPipe[1]);
}

EventLoop::SharedPtr EventLoop::eventLoop()
{
    EventLoop::SharedPtr loop = localEventLoop().lock();
    if (!loop) {
        std::lock_guard<std::mutex> locker(mainMutex);
        loop = mainLoop.lock();
    }
    return loop;
}


void EventLoop::error(const char* err)
{
    fprintf(stderr, "%s\n", err);
    abort();
}

void EventLoop::post(Event* event)
{
    std::lock_guard<std::mutex> locker(mutex);
    events.push(event);
    wakeup();
}

void EventLoop::wakeup()
{
    if (std::this_thread::get_id() == threadId)
        return;

    char b = 'w';
    int w;
    eintrwrap(w, ::write(eventPipe[1], &b, 1));
}

void EventLoop::quit(int code)
{
    std::lock_guard<std::mutex> locker(mutex);
    if (!mExecStack.empty()) {
        stop = true;
        exitCode = code;
        if (std::this_thread::get_id() != threadId)
            wakeup();
    }
}

inline bool EventLoop::sendPostedEvents()
{
    std::unique_lock<std::mutex> locker(mutex);
    if (events.empty())
        return false;
    while (!events.empty()) {
        auto event = events.front();
        events.pop();
        locker.unlock();
        event->exec();
        delete event;
        locker.lock();
    }
    return true;
}

// milliseconds
static inline uint64_t currentTime()
{
#if defined(HAVE_CLOCK_MONOTONIC_RAW) || defined(HAVE_CLOCK_MONOTONIC)
    timespec now;
#if defined(HAVE_CLOCK_MONOTONIC_RAW)
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &now) == -1)
        return 0;
#elif defined(HAVE_CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
        return 0;
#endif
    const uint64_t t = (now.tv_sec * 1000LLU) + (now.tv_nsec / 1000000LLU);
#elif defined(HAVE_MACH_ABSOLUTE_TIME)
    static mach_timebase_info_data_t info;
    static bool first = true;
    uint64_t t = mach_absolute_time();
    if (first) {
        first = false;
        mach_timebase_info(&info);
    }
    t = t * info.numer / (info.denom * 1000); // microseconds
    t /= 1000; // milliseconds
#else
#error No time getting mechanism
#endif
    return t;
}

int EventLoop::registerTimer(std::function<void(int)>&& func, int timeout, int flags)
{
    std::lock_guard<std::mutex> locker(mutex);
    {
        TimerData data;
        do {
            data.id = ++nextTimerId;
        } while (timersById.count(&data));
    }
    TimerData* timer = new TimerData(currentTime() + timeout, nextTimerId, flags, timeout, std::forward<std::function<void(int)> >(func));
    timersByTime.insert(timer);
    timersById.insert(timer);
    assert(timersById.count(timer) == 1);
    wakeup();
    return nextTimerId;
}

void EventLoop::unregisterTimer(int id)
{
    // rather slow
    std::lock_guard<std::mutex> locker(mutex);
    clearTimer(id);
}

void EventLoop::clearTimer(int id)
{
    TimerData* t = 0;
    {
        TimerData data;
        data.id = id;
        auto timer = timersById.find(&data);
        if (timer == timersById.end()) {
            // no such timer
            return;
        }
        t = *timer;
        assert(timersById.count(t) == 1);
        timersById.erase(timer);
        assert(timersById.count(t) == 0);
    }
    assert(t);
    const auto range = timersByTime.equal_range(t);
    auto timer = range.first;
    const auto end = range.second;
    while (timer != end) {
        if (*timer == t) {
            // got it
            timersByTime.erase(timer);
            delete t;
            return;
        }
        ++timer;
    }
    fprintf(stderr, "Found timer %d by id but not by timeout\n", t->id);
    abort();
}

inline bool EventLoop::sendTimers()
{
    std::set<uint64_t> fired;
    std::unique_lock<std::mutex> locker(mutex);
    const uint64_t now = currentTime();
    for (;;) {
        auto timer = timersByTime.begin();
        if (timer == timersByTime.end())
            return !fired.empty();
        TimerData* timerData = *timer;
        int currentId = timerData->id;
        while (fired.count(currentId)) {
            // already fired this round, ignore for now
            ++timer;
            if (timer == timersByTime.end())
                return !fired.empty();
            timerData = *timer;
            currentId = timerData->id;
        }
        if (timerData->when > now)
            return !fired.empty();
        if (timerData->flags & Timer::SingleShot) {
            // remove the timer before firing
            std::function<void(int)> func = std::move(timerData->callback);
            timersByTime.erase(timer);
            timersById.erase(timerData);
            delete timerData;
            fired.insert(currentId);

            // fire
            locker.unlock();
            func(currentId);
            locker.lock();
        } else {
            // silly std::set/multiset doesn't have a way of forcing a resort.
            // take the item out and reinsert it

            // luckily we have the exact iterator
            timersByTime.erase(timer);

            // update the fire time
            const int64_t overtime = now - timerData->when;
            timerData->when = (now - overtime) + timerData->interval;

            // reinsert with the updated time
            timersByTime.insert(timerData);

            // take a copy of the callback in case the timer gets
            // removed before we get a chance to call it
            // ### is this heavy?
            std::function<void(int)> cb = timerData->callback;
            fired.insert(currentId);

            // fire
            locker.unlock();
            cb(currentId);
            locker.lock();
        }
    }
    return !fired.empty();
}

void EventLoop::registerSocket(int fd, int mode, std::function<void(int, int)>&& func)
{
    std::lock_guard<std::mutex> locker(mutex);
    sockets[fd] = std::make_pair(mode, std::forward<std::function<void(int, int)> >(func));

    int e;
#if defined(HAVE_EPOLL)
    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLET|EPOLLRDHUP;
    if (mode & SocketRead)
        ev.events |= EPOLLIN;
    if (mode & SocketWrite)
        ev.events |= EPOLLOUT;
    if (mode & SocketOneShot)
        ev.events |= EPOLLONESHOT;
    ev.data.fd = fd;
    e = epoll_ctl(pollFd, EPOLL_CTL_ADD, fd, &ev);
#elif defined(HAVE_KQUEUE)
    const struct { int rf; int kf; } flags[] = {
        { SocketRead, EVFILT_READ },
        { SocketWrite, EVFILT_WRITE },
        { 0, 0 }
    };
    for (int i = 0; flags[i].rf; ++i) {
        if (!(mode & flags[i].rf))
            continue;
        struct kevent ev;
        memset(&ev, '\0', sizeof(struct kevent));
        ev.ident = fd;
        ev.flags = EV_ADD|EV_ENABLE;
        if (mode & SocketOneShot)
            ev.flags |= EV_ONESHOT;
        ev.filter = flags[i].kf;
        eintrwrap(e, kevent(pollFd, &ev, 1, 0, 0, 0));
    }
#endif
    if (e == -1) {
        if (errno != EEXIST) {
            char buf[128];
            STRERROR_R(errno, buf, sizeof(buf));
            fprintf(stderr, "Unable to register socket %d with mode %x: %d (%s)\n", fd, mode, errno, buf);
        }
    }
}

void EventLoop::updateSocket(int fd, int mode)
{
    std::lock_guard<std::mutex> locker(mutex);
    std::map<int, std::pair<int, std::function<void(int, int)> > >::iterator socket = sockets.find(fd);
    if (socket == sockets.end()) {
        fprintf(stderr, "Unable to find socket to update %d\n", fd);
        return;
    }
#if defined(HAVE_KQUEUE)
    const int oldMode = socket->second.first;
#endif
    socket->second.first = mode;

    int e;
#if defined(HAVE_EPOLL)
    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLET|EPOLLRDHUP;
    if (mode & SocketRead)
        ev.events |= EPOLLIN;
    if (mode & SocketWrite)
        ev.events |= EPOLLOUT;
    if (mode & SocketOneShot)
        ev.events |= EPOLLONESHOT;
    ev.data.fd = fd;
    e = epoll_ctl(pollFd, EPOLL_CTL_MOD, fd, &ev);
#elif defined(HAVE_KQUEUE)
    const struct { int rf; int kf; } flags[] = {
        { SocketRead, EVFILT_READ },
        { SocketWrite, EVFILT_WRITE },
        { 0, 0 }
    };
    for (int i = 0; flags[i].rf; ++i) {
        if (!(mode & flags[i].rf) && !(oldMode & flags[i].rf))
            continue;
        struct kevent ev;
        memset(&ev, '\0', sizeof(struct kevent));
        ev.ident = fd;
        if (mode & flags[i].rf) {
            ev.flags = EV_ADD|EV_ENABLE;
            if (mode & SocketOneShot)
                ev.flags |= EV_ONESHOT;
        } else {
            assert(oldMode & flags[i].rf);
            ev.flags = EV_DELETE|EV_DISABLE;
        }
        ev.filter = flags[i].kf;
        eintrwrap(e, kevent(pollFd, &ev, 1, 0, 0, 0));
    }
#endif
    if (e == -1) {
        if (errno != EEXIST && errno != ENOENT) {
            char buf[128];
            STRERROR_R(errno, buf, sizeof(buf));
            fprintf(stderr, "Unable to register socket %d with mode %x: %d (%s)\n", fd, mode, errno, buf);
        }
    }
}

void EventLoop::unregisterSocket(int fd)
{
    std::lock_guard<std::mutex> locker(mutex);
    auto socket = sockets.find(fd);
    if (socket == sockets.end())
        return;
#ifdef HAVE_KQUEUE
    const int mode = socket->second.first;
#endif
    sockets.erase(socket);

    int e;
#if defined(HAVE_EPOLL)
    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    e = epoll_ctl(pollFd, EPOLL_CTL_DEL, fd, &ev);
#elif defined(HAVE_KQUEUE)
    const struct { int rf; int kf; } flags[] = {
        { SocketRead, EVFILT_READ },
        { SocketWrite, EVFILT_WRITE },
        { 0, 0 }
    };
    for (int i = 0; flags[i].rf; ++i) {
        if (!(mode & flags[i].rf))
            continue;
        struct kevent ev;
        memset(&ev, '\0', sizeof(struct kevent));
        ev.ident = fd;
        ev.flags = EV_DELETE|EV_DISABLE;
        ev.filter = flags[i].kf;
        eintrwrap(e, kevent(pollFd, &ev, 1, 0, 0, 0));
    }
#endif
    if (e == -1) {
        if (errno != ENOENT) {
            char buf[128];
            STRERROR_R(errno, buf, sizeof(buf));
            fprintf(stderr, "Unable to unregister socket %d: %d (%s)\n", fd, errno, buf);
        }
    }
}

int EventLoop::exec(int timeoutTime)
{
    int quitTimerId = -1;
    if (timeoutTime != -1)
        quitTimerId = registerTimer(std::bind(&EventLoop::quit, this, Timeout), timeoutTime, Timer::SingleShot);
    {
        std::lock_guard<std::mutex> locker(mutex);
        ExecContext ctx;
        ctx.quitTimerId = quitTimerId;
        mExecStack.push_back(ctx);
    }

    int i, e, mode;

    enum { MaxEvents = 64 };

#if defined(HAVE_EPOLL)
    epoll_event events[MaxEvents];
#elif defined(HAVE_KQUEUE)
    struct kevent events[MaxEvents];
    timespec timeout;
#endif

    for (;;) {
        for (;;) {
            if (!sendPostedEvents() && !sendTimers())
                break;
        }
        int waitUntil = -1;
        {
            std::lock_guard<std::mutex> locker(mutex);

            // check if we're stopped
            if (stop) {
                stop = false;
                if (mExecStack.back().quitTimerId != -1)
                    clearTimer(mExecStack.back().quitTimerId);
                mExecStack.pop_back();
                return exitCode;
            }

            const auto timer = timersByTime.begin();
            if (timer != timersByTime.end()) {
                const uint64_t now = currentTime();
                waitUntil = std::max<int>((*timer)->when - now, 0);
            }
        }
        int eventCount;
#if defined(HAVE_EPOLL)
        eintrwrap(eventCount, epoll_wait(pollFd, events, MaxEvents, waitUntil));
#elif defined(HAVE_KQUEUE)
        timespec* timeptr = 0;
        if (waitUntil != -1) {
            timeout.tv_sec = waitUntil / 1000;
            timeout.tv_nsec = (waitUntil % 1000LLU) * 1000000;
            timeptr = &timeout;
        }
        eintrwrap(eventCount, kevent(pollFd, 0, 0, events, MaxEvents, timeptr));
#endif
        if (eventCount < 0) {
            // bad
            std::lock_guard<std::mutex> locker(mutex);
            while (!mExecStack.empty()) {
                if (mExecStack.back().quitTimerId != -1)
                    clearTimer(mExecStack.back().quitTimerId);
                mExecStack.pop_back();
            }
            return GeneralError;
        } else if (eventCount) {
            std::unique_lock<std::mutex> locker(mutex);
            std::map<int, std::pair<int, std::function<void(int, int)> > > local = sockets;
            locker.unlock();
            for (i = 0; i < eventCount; ++i) {
                mode = 0;
#if defined(HAVE_EPOLL)
                const uint32_t ev = events[i].events;
                const int fd = events[i].data.fd;
                if (ev & (EPOLLERR|EPOLLHUP) && !(ev & EPOLLRDHUP)) {
                    // bad, take the fd out
                    epoll_ctl(pollFd, EPOLL_CTL_DEL, fd, &events[i]);
                    locker.lock();
                    sockets.erase(fd);
                    locker.unlock();
                    if (ev & EPOLLERR) {
                        char buf[128];
                        int err;
                        socklen_t size = sizeof(err);
                        e = ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &size);
                        if (e == -1) {
                            STRERROR_R(errno, buf, sizeof(buf));
                            fprintf(stderr, "Error getting error for fd %d: %d (%s)\n", fd, errno, buf);
                        } else {
                            STRERROR_R(errno, buf, sizeof(buf));
                            fprintf(stderr, "Error on socket %d, removing: %d (%s)\n", fd, err, buf);
                        }
                    }
                    if (ev & EPOLLHUP) {
                        fprintf(stderr, "HUP on socket %d, removing\n", fd);
                    }
                    auto socket = local.find(fd);
                    if (socket != local.end())
                        socket->second.second(fd, SocketError);
                    continue;
                }
                if (ev & (EPOLLIN|EPOLLRDHUP)) {
                    // read
                    mode |= SocketRead;
                }
                if (ev & EPOLLOUT) {
                    // write
                    mode |= SocketWrite;
                }
#elif defined(HAVE_KQUEUE)
                const int16_t filter = events[i].filter;
                const uint16_t flags = events[i].flags;
                const int fd = events[i].ident;
                if (flags & EV_ERROR) {
                    // bad, take the fd out
                    struct kevent& kev = events[i];
                    const int err = kev.data;
                    kev.flags = EV_DELETE|EV_DISABLE;
                    kevent(pollFd, &kev, 1, 0, 0, 0);
                    locker.lock();
                    sockets.erase(fd);
                    locker.unlock();
                    {
                        char buf[128];
                        STRERROR_R(errno, buf, sizeof(buf));
                        fprintf(stderr, "Error on socket %d, removing: %d (%s)\n", fd, err, buf);
                    }
                    auto socket = local.find(fd);
                    if (socket != local.end())
                        socket->second.second(fd, SocketError);
                    continue;
                }
                if (filter == EVFILT_READ)
                    mode |= SocketRead;
                else if (filter == EVFILT_WRITE)
                    mode |= SocketWrite;
#endif
                if (mode) {
                    if (fd == eventPipe[0]) {
                        // drain the pipe
                        char q;
                        do {
                            eintrwrap(e, ::read(eventPipe[0], &q, 1));
                            if (e == 1 && q == 'q') {
                                // signal caught, we need to shut down
                                fprintf(stderr, "Caught Ctrl-C\n");
                                std::lock_guard<std::mutex> locker(mutex);
                                while (!mExecStack.empty()) {
                                    if (mExecStack.back().quitTimerId != -1)
                                        clearTimer(mExecStack.back().quitTimerId);
                                    mExecStack.pop_back();
                                }
                                return Success;
                            }
                        } while (e == 1);
                        if (e == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                            // error
                            char buf[128];
                            STRERROR_R(errno, buf, sizeof(buf));
                            fprintf(stderr, "Error reading from event pipe: %d (%s)\n", errno, buf);
                            std::lock_guard<std::mutex> locker(mutex);
                            while (!mExecStack.empty()) {
                                if (mExecStack.back().quitTimerId != -1)
                                    clearTimer(mExecStack.back().quitTimerId);
                                mExecStack.pop_back();
                            }
                            return GeneralError;
                        }
                    } else {
                        auto socket = local.find(fd);
                        if (socket == local.end()) {
                            // could happen if the socket is unregistered right after epoll_wait has woken up
                            // and just before we've taken the local copy of the sockets map

                            // really make sure it's gone
                            locker.lock();
                            socket = sockets.find(fd);
                            if (socket == sockets.end()) {
#if defined(HAVE_EPOLL)
                                epoll_ctl(pollFd, EPOLL_CTL_DEL, fd, &events[i]);
#elif defined(HAVE_KQUEUE)
                                // this is a bit awful, the mode is not known at this point so we'll just have to try to remove it
                                const int kf[] = { EVFILT_READ, EVFILT_WRITE };
                                for (int i = 0; i < 2; ++i) {
                                    struct kevent ev;
                                    memset(&ev, '\0', sizeof(struct kevent));
                                    ev.ident = fd;
                                    ev.flags = EV_DELETE|EV_DISABLE;
                                    ev.filter = kf[i];
                                    eintrwrap(e, kevent(pollFd, &ev, 1, 0, 0, 0));
                                }
#endif
                                locker.unlock();
                                continue;
                            }
                            locker.unlock();
                        }
                        socket->second.second(fd, mode);
                    }
                }
            }
        }
    }

    assert(0);
    return GeneralError;
}
