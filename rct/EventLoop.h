#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <queue>
#include <set>
#include <unordered_set>
#include "Apply.h"

class Event
{
public:
    enum Type {
        Type_SignalEvent = 1,
        Type_DeleteLater
    };
    Event(int typ) : type(typ) {}
    virtual ~Event() { }
    virtual void exec() = 0;
    const int type;
};

template<typename Object, typename... Args>
class SignalEvent : public Event
{
public:
    enum MoveType { Move };

    SignalEvent(Object& o, Args&&... a)
        : Event(Type_SignalEvent), obj(o), args(a...)
    {
    }
    SignalEvent(Object&& o, Args&&... a)
        : Event(Type_SignalEvent), obj(o), args(a...)
    {
    }
    SignalEvent(Object& o, MoveType, Args&&... a)
        : Event(Type_SignalEvent), obj(o), args(std::move(a...))
    {
    }
    SignalEvent(Object&& o, MoveType, Args&&... a)
        : Event(Type_SignalEvent), obj(o), args(std::move(a...))
    {
    }

    void exec() { applyMove(obj, args); }

private:
    Object obj;
    std::tuple<Args...> args;
};

template<typename T>
class DeleteLaterEvent : public Event
{
public:
    DeleteLaterEvent(T* d)
        : Event(Type_DeleteLater), del(d)
    {
    }

    void exec() { delete del; }

private:
    T* del;
};

class EventLoop : public std::enable_shared_from_this<EventLoop>
{
public:
    typedef std::shared_ptr<EventLoop> SharedPtr;
    typedef std::weak_ptr<EventLoop> WeakPtr;

    EventLoop();
    ~EventLoop();

    enum Flag {
        None = 0x0,
        MainEventLoop = 0x1,
        EnableSigIntHandler = 0x2
    };
    enum PostType {
        Move = 1,
        Async
    };

    void init(unsigned flags = None);

    unsigned flags() const { return flgs; }

    template<typename T>
    static void deleteLater(T* del)
    {
        if (EventLoop::SharedPtr loop = eventLoop()) {
            loop->post(new DeleteLaterEvent<T>(del));
        } else {
            error("No event loop!");
        }
    }
    template<typename Object, typename... Args>
    void post(Object& object, Args&&... args)
    {
        post(new SignalEvent<Object, Args...>(object, std::forward<Args>(args)...));
    }
    template<typename Object, typename... Args>
    void postMove(Object& object, Args&&... args)
    {
        post(new SignalEvent<Object, Args...>(object, SignalEvent<Object, Args...>::Move, std::forward<Args>(args)...));
    }
    template<typename Object, typename... Args>
    void callLater(Object&& object, Args&&... args)
    {
        post(new SignalEvent<Object, Args...>(std::forward<Object>(object), std::forward<Args>(args)...));
    }
    template<typename Object, typename... Args>
    void callLaterMove(Object&& object, Args&&... args)
    {
        post(new SignalEvent<Object, Args...>(std::forward<Object>(object), SignalEvent<Object, Args...>::Move, std::forward<Args>(args)...));
    }
    void post(Event* event);
    void wakeup();

    enum Mode {
        SocketRead = 0x1,
        SocketWrite = 0x2,
        SocketOneShot = 0x4,
        SocketError = 0x8
    };
    void registerSocket(int fd, int mode, std::function<void(int, int)>&& func);
    void updateSocket(int fd, int mode);
    void unregisterSocket(int fd);

    // See Timer.h for the flags
    int registerTimer(std::function<void(int)>&& func, int timeout, int flags = 0);
    void unregisterTimer(int id);

    enum { Success, GeneralError = -1, Timeout = -2 };
    int exec(int timeout = -1);
    void quit(int code = 0);

    bool isRunning() const { std::lock_guard<std::mutex> locker(mutex); return execLevel; }

    static EventLoop::SharedPtr mainEventLoop() { std::lock_guard<std::mutex> locker(mainMutex); return mainLoop.lock(); }
    static EventLoop::SharedPtr eventLoop();

    static bool isMainThread() { return EventLoop::mainEventLoop() && std::this_thread::get_id() == EventLoop::mainEventLoop()->threadId; }
private:
    bool sendPostedEvents();
    bool sendTimers();
    void cleanup();

    static void error(const char* err);

private:
    static std::mutex mainMutex;
    mutable std::mutex mutex;
    std::thread::id threadId;

    std::queue<Event*> events;
    int eventPipe[2];
    int pollFd;

    std::map<int, std::pair<int, std::function<void(int, int)> > > sockets;

    class TimerData
    {
    public:
        TimerData() { }
        TimerData(uint64_t w, int i, int f, int in, std::function<void(int)>&& cb)
            : when(w), id(i), flags(f), interval(in), callback(std::move(cb))
        {
        }
        TimerData(TimerData&& other)
            : when(other.when), id(other.id), flags(other.flags),
              interval(other.interval), callback(std::move(other.callback))
        {
        }
        TimerData& operator=(TimerData&& other)
        {
            when = other.when;
            id = other.id;
            flags = other.flags;
            interval = other.interval;
            callback = std::move(other.callback);
            return *this;
        }

        bool operator<(const TimerData& other) const
        {
            return when < other.when;
        }

        uint64_t when;
        uint32_t id;
        int flags, interval;
        std::function<void(int)> callback;

    private:
        TimerData(const TimerData& other) = delete;
        TimerData& operator=(const TimerData& other) = delete;
    };

    struct TimerDataSet
    {
        bool operator()(TimerData* a, TimerData* b) const { return a->when < b->when; }
    };
    struct TimerDataHash
    {
        // two operators for the price of one!
        size_t operator()(TimerData* a) const { return a->id; }
        bool operator()(TimerData* a, TimerData* b) const { return a->id == b->id; }
    };
    typedef std::multiset<TimerData*, TimerDataSet> TimersByTime;
    typedef std::unordered_set<TimerData*, TimerDataHash, TimerDataHash> TimersById;
    TimersByTime timersByTime;
    TimersById timersById;
    uint32_t nextTimerId;

    bool stop;
    int exitCode, execLevel;

    static EventLoop::WeakPtr mainLoop;

    unsigned flgs;

private:
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;
};

#endif
