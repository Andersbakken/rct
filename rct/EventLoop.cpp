#include "EventLoop.h"
#include "SocketClient.h"
#include "Timer.h"
#include "Rct.h"
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
#include <sys/stat.h>
#include <pthread.h>
#include <stdlib.h>
#ifdef HAVE_MACH_ABSOLUTE_TIME
#  include <mach/mach.h>
#  include <mach/mach_time.h>
#endif
#if defined(RCT_EVENTLOOP_CALLBACK_TIME_THRESHOLD) && RCT_EVENTLOOP_CALLBACK_TIME_THRESHOLD > 0
#  include "StopWatch.h"
#  include "Log.h"
#define CALLBACK(op)                                                \
    do {                                                            \
        StopWatch sw;                                               \
        op;                                                         \
        if (sw.elapsed() >= RCT_EVENTLOOP_CALLBACK_TIME_THRESHOLD)  \
            ::error() << "callback took" << sw.elapsed()            \
                      << "ms\n" << Rct::backtrace();                \
    } while (0)
#else
#define CALLBACK(op) op
#endif

struct RctStrError
{
    char buf[1024];
};

static std::once_flag errorOnce;

char* rct_strerror(int errn)
{
    static pthread_key_t errorKey;
    std::call_once(errorOnce, [](){
            pthread_key_create(&errorKey, 0);
        });
    RctStrError* err = static_cast<RctStrError*>(pthread_getspecific(errorKey));
    if (!err) {
        err = new RctStrError;
        pthread_setspecific(errorKey, err);
    }
#ifdef _GNU_SOURCE
    return strerror_r(errn, err->buf, sizeof(err->buf));
#else
    strerror_r(errn, err->buf, sizeof(err->buf));
    return err->buf;
#endif
}

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

static void signalHandler(int sig)
{
    char b = 'q';
    int w;
    const int pipe = mainEventPipe;
    if (pipe != -1)
        eintrwrap(w, ::write(pipe, &b, 1));
}

EventLoop::EventLoop()
    :
#if defined(HAVE_EPOLL) || defined(HAVE_KQUEUE)
    pollFd(-1),
#endif
    nextTimerId(0), stop(false), timeout(false), flgs(0)
{
    std::call_once(mainOnce, [](){
            mainEventPipe = -1;
            pthread_key_create(&eventLoopKey, 0);
            signal(SIGPIPE, SIG_IGN);
        });
}

EventLoop::~EventLoop()
{
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
    if (!SocketClient::setFlags(eventPipe[0], O_NONBLOCK, F_GETFL, F_SETFL)) {
        cleanup();
        return;
    }

#if defined(HAVE_EPOLL)
    pollFd = epoll_create1(0);
#elif defined(HAVE_KQUEUE)
    pollFd = kqueue();
#elif defined(HAVE_SELECT)
    // just to avoid the #error below
#else
#error No supported event polling mechanism
#endif
#if defined(HAVE_EPOLL) || defined(HAVE_KQUEUE)
    if (pollFd == -1) {
        cleanup();
        return;
    }
#endif

#if !defined(HAVE_SELECT)
    NativeEvent ev;
#endif
#if defined(HAVE_EPOLL)
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = eventPipe[0];
    e = epoll_ctl(pollFd, EPOLL_CTL_ADD, eventPipe[0], &ev);
#elif defined(HAVE_KQUEUE)
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
        assert(mainEventPipe == -1);
        mainEventPipe = eventPipe[1];
        struct sigaction act;
        memset (&act, '\0', sizeof(act));
        act.sa_handler = signalHandler;
        if (::sigaction(SIGINT, &act, 0) == -1) {
            cleanup();
            return;
        }
    }

    std::shared_ptr<EventLoop> that = shared_from_this();
    localEventLoop() = that;
    if (flags & MainEventLoop) {
        std::lock_guard<std::mutex> locker(mainMutex);
        mainLoop = that;
    }
}

void EventLoop::cleanup()
{
    std::lock_guard<std::mutex> locker(mutex);
    localEventLoop().reset();

    while (!events.empty()) {
        delete events.front();
        events.pop();
    }

#if defined(HAVE_EPOLL) || defined(HAVE_KQUEUE)
    if (pollFd != -1)
        ::close(pollFd);
#endif

    if (eventPipe[0] != -1)
        ::close(eventPipe[0]);
    if (eventPipe[1] != -1)
        ::close(eventPipe[1]);
    if (flgs & MainEventLoop)
        mainLoop.reset();
    if (flgs & EnableSigIntHandler) {
        assert(mainEventPipe >= 0);
        mainEventPipe = -1;
    }
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

void EventLoop::quit()
{
    std::lock_guard<std::mutex> locker(mutex);
    stop = true;
    wakeup();
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

int EventLoop::registerTimer(std::function<void(int)>&& func, int timeout, unsigned int flags)
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
            CALLBACK(func(currentId));
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
            CALLBACK(cb(currentId));
            locker.lock();
        }
    }
    return !fired.empty();
}

bool EventLoop::registerSocket(int fd, unsigned int mode, std::function<void(int, unsigned int)>&& func)
{
    std::lock_guard<std::mutex> locker(mutex);
    sockets[fd] = std::make_pair(mode, std::forward<std::function<void(int, unsigned int)> >(func));

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
    e = 0;
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
#elif defined(HAVE_SELECT)
    e = 0; // fake ok
    wakeup();
#endif
    if (e == -1) {
        if (errno != EEXIST) {
            fprintf(stderr, "Unable to register socket %d with mode %x: %d (%s)\n", fd, mode, errno, rct_strerror(errno));
            return false;
        }
    }
    return true;
}

bool EventLoop::updateSocket(int fd, unsigned int mode)
{
    std::lock_guard<std::mutex> locker(mutex);
    auto socket = sockets.find(fd);
    if (socket == sockets.end()) {
        fprintf(stderr, "Unable to find socket to update %d\n", fd);
        return false;
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
    e = 0;
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
#elif defined(HAVE_SELECT)
    e = 0; // fake ok
    wakeup();
#endif
    if (e == -1) {
        if (errno != EEXIST && errno != ENOENT) {
            fprintf(stderr, "Unable to register socket %d with mode %x: %d (%s)\n", fd, mode, errno, rct_strerror(errno));
            return false;
        }
    }
    return true;
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
    e = 0;
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
#elif defined(HAVE_SELECT)
    e = 0; // fake ok
    wakeup();
#endif
    if (e == -1) {
        if (errno != ENOENT) {
            fprintf(stderr, "Unable to unregister socket %d: %d (%s)\n", fd, errno, rct_strerror(errno));
        }
    }
}

unsigned int EventLoop::processSocket(int fd, int timeout)
{
    int eventCount;
#if defined(HAVE_EPOLL) || defined(HAVE_KQUEUE)
    enum { MaxEvents = 2 };
    NativeEvent events[MaxEvents];
#endif

#if defined(HAVE_EPOLL)
    int processFd = epoll_create1(0);

    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLET|EPOLLRDHUP|EPOLLIN|EPOLLOUT;
    ev.data.fd = fd;
    epoll_ctl(processFd, EPOLL_CTL_ADD, fd, &ev);

    eintrwrap(eventCount, epoll_wait(processFd, events, MaxEvents, timeout));
#elif defined(HAVE_KQUEUE)
    int processFd = kqueue(), e;

    const struct { int rf; int kf; } flags[] = {
        { SocketRead, EVFILT_READ },
        { SocketWrite, EVFILT_WRITE },
        { 0, 0 }
    };
    for (int i = 0; flags[i].rf; ++i) {
        struct kevent ev;
        memset(&ev, '\0', sizeof(struct kevent));
        ev.ident = fd;
        ev.flags = EV_ADD|EV_ENABLE;
        ev.filter = flags[i].kf;
        eintrwrap(e, kevent(processFd, &ev, 1, 0, 0, 0));
    }

    timespec time;
    if (timeout != -1) {
        time.tv_sec = timeout / 1000;
        time.tv_nsec = (timeout % 1000LLU) * 1000000;
    }
    eintrwrap(eventCount, kevent(processFd, 0, 0, events, MaxEvents, (timeout == -1) ? 0 : &time));
#elif defined(HAVE_SELECT)
    fd_set rdfd, wrfd;
    FD_ZERO(&rdfd);
    FD_ZERO(&wrfd);
    FD_SET(fd, &rdfd);
    FD_SET(fd, &wrfd);

    timeval time;
    if (timeout != -1) {
        time.tv_sec = timeout / 1000;
        time.tv_usec = (timeout % 1000LLU) * 1000;
    }
    eintrwrap(eventCount, select(fd + 1, &rdfd, &wrfd, 0, (timeout == -1) ? 0 : &time));
#endif
    if (eventCount == -1)
        fprintf(stderr, "processSocket returned -1 (%d)\n", errno);
    if (eventCount == 0)
        return 0;

#if defined(HAVE_EPOLL) || defined(HAVE_KQUEUE)
    ::close(processFd);
#elif defined(HAVE_SELECT)
    NativeEvent event;
    event.rdfd = &rdfd;
    event.wrfd = &wrfd;
    NativeEvent* events = &event;
#endif
    return processSocketEvents(events, eventCount);
}

unsigned int EventLoop::fireSocket(int fd, unsigned int mode)
{
    std::unique_lock<std::mutex> locker(mutex);
    const auto socket = sockets.find(fd);
    if (socket != sockets.end()) {
        const auto callback = socket->second.second;
        locker.unlock();
        CALLBACK(callback(fd, mode));
        return mode;
    }
    return 0;
}

unsigned int EventLoop::processSocketEvents(NativeEvent* events, int eventCount)
{
    unsigned int all = 0;
    int e;

#if defined(HAVE_SELECT)
    std::map<int, std::pair<unsigned int, std::function<void(int, unsigned int)> > > local;
    {
#warning this is not optimal
        std::lock_guard<std::mutex> locker(mutex);
        local = sockets;
    }
    auto socket = local.begin();
    if (socket == local.end()) {
        fprintf(stderr, "wanted to processSocketEvents but no sockets present\n");
        return 0;
    }
#endif

    for (int i = 0; i < eventCount; ++i) {
        unsigned int mode = 0;
#if defined(HAVE_EPOLL)
        const uint32_t ev = events[i].events;
        const int fd = events[i].data.fd;
        if (ev & (EPOLLERR|EPOLLHUP) && !(ev & EPOLLRDHUP)) {
            // bad, take the fd out
            epoll_ctl(pollFd, EPOLL_CTL_DEL, fd, &events[i]);
            {
                std::lock_guard<std::mutex> locker(mutex);
                sockets.erase(fd);
            }
            if (ev & EPOLLERR) {
                int err;
                socklen_t size = sizeof(err);
                e = ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &size);
                if (e == -1) {
                    fprintf(stderr, "Error getting error for fd %d: %d (%s)\n", fd, errno, rct_strerror(errno));
                } else {
                    fprintf(stderr, "Error on socket %d, removing: %d (%s)\n", fd, err, rct_strerror(errno));
                }
                mode |= SocketError;
            }
            if (ev & EPOLLHUP) {
                // check if our fd is a socket
                struct stat st;
                if (fstat(fd, &st) != -1 && S_ISSOCK(st.st_mode)) {
                    mode |= SocketError;
                    fprintf(stderr, "HUP on socket %d, removing\n", fd);
                } else {
                    mode |= SocketRead;
                }
            }

            all |= fireSocket(fd, mode);
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
            {
                std::lock_guard<std::mutex> locker(mutex);
                sockets.erase(fd);
            }
            fprintf(stderr, "Error on socket %d, removing: %d (%s)\n", fd, err, rct_strerror(errno));

            all |= fireSocket(fd, SocketError);
            continue;
        }
        if (filter == EVFILT_READ)
            mode |= SocketRead;
        else if (filter == EVFILT_WRITE)
            mode |= SocketWrite;
#elif defined(HAVE_SELECT)
        // iterate through the sockets until we find one in either fd_set
        int fd = -1;
        //assert(socket != local.end());
        while (socket != local.end()) {
            if (FD_ISSET(socket->first, events->rdfd)) {
                // go
                fd = socket->first;
                mode |= SocketRead;
                ++socket;
                break;
            }
            if (events->wrfd && FD_ISSET(socket->first, events->wrfd)) {
                // go
                fd = socket->first;
                mode |= SocketWrite;
                ++socket;
                break;
            }
            ++socket;
        }
        if (fd == -1) {
            if (FD_ISSET(eventPipe[0], events->rdfd)) {
                fd = eventPipe[0];
                mode |= SocketRead;
            }
        }
        //printf("firing %d (%d/%d)\n", fd, i, eventCount);
#endif
        if (mode) {
            if (fd == eventPipe[0]) {
                // drain the pipe
                char q;
                do {
                    eintrwrap(e, ::read(eventPipe[0], &q, 1));
                    if (e == 1 && q == 'q') {
                        // signal caught, we need to shut down
                        return Success;
                    }
                } while (e == 1);
                if (e == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    // error
                    fprintf(stderr, "Error reading from event pipe: %d (%s)\n", errno, rct_strerror(errno));
                    return GeneralError;
                }
            } else {
                all |= fireSocket(fd, mode);
            }
        }
    }
    return all;
}

unsigned int EventLoop::exec(int timeoutTime)
{
    int quitTimerId = -1;
    if (timeoutTime != -1)
        quitTimerId = registerTimer([=](int) { timeout = true; quit(); }, timeoutTime, Timer::SingleShot);

    unsigned int ret = 0;

#if defined(HAVE_EPOLL) || defined(HAVE_KQUEUE)
    enum { MaxEvents = 64 };
    NativeEvent events[MaxEvents];
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
                if (timeout) {
                    timeout = false;
                    ret = Timeout;
                } else {
                    ret = Success;
                }
                break;
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
        timespec timeout;
        timespec* timeptr = 0;
        if (waitUntil != -1) {
            timeout.tv_sec = waitUntil / 1000;
            timeout.tv_nsec = (waitUntil % 1000LLU) * 1000000;
            timeptr = &timeout;
        }
        eintrwrap(eventCount, kevent(pollFd, 0, 0, events, MaxEvents, timeptr));
#elif defined(HAVE_SELECT)
        timeval timeout;
        timeval* timeptr = 0;
        if (waitUntil != -1) {
            timeout.tv_sec = waitUntil / 1000;
            timeout.tv_usec = (waitUntil % 1000LLU) * 1000;
            timeptr = &timeout;
        }

        fd_set rdfd, wrfd;
        fd_set* wrfdp = 0;
        FD_ZERO(&rdfd);
        FD_ZERO(&wrfd);
        int max = eventPipe[0];
        FD_SET(max, &rdfd);
        {
            std::lock_guard<std::mutex> locker(mutex);
            auto s = sockets.begin();
            const auto e = sockets.end();
            while (s != e) {
                if (s->second.first & SocketRead) {
                    FD_SET(s->first, &rdfd);
                }
                if (s->second.first & SocketWrite) {
                    if (!wrfdp)
                        wrfdp = &wrfd;
                    FD_SET(s->first, wrfdp);
                }
                max = std::max(max, s->first);
                ++s;
            }
        }

        eintrwrap(eventCount, select(max + 1, &rdfd, wrfdp, 0, timeptr));
#endif
        if (eventCount < 0) {
            // bad
            ret = GeneralError;
            break;
        } else if (eventCount) {
#if defined(HAVE_SELECT)
            NativeEvent event;
            event.rdfd = &rdfd;
            event.wrfd = wrfdp;
            NativeEvent* events = &event;
#endif
            ret = processSocketEvents(events, eventCount);
            if (ret & (Success|GeneralError|Timeout))
                break;
        }
    }

    if (quitTimerId != -1)
        clearTimer(quitTimerId);
    return ret;
}
