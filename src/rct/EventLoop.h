#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <rct/Mutex.h>
#include <rct/Thread.h>
#include <deque>
#include <sys/time.h>

class Event;
class EventReceiver;
struct timeval;

class EventLoop
{
public:
    EventLoop();
    ~EventLoop();

    static EventLoop* instance();

    enum Flag {
        Read = 0x1,
        Write = 0x2,
        Disconnected = 0x4
    };
    typedef void(*FdFunc)(int, unsigned int, void*);
    typedef void(*TimerFunc)(int, void*);

    int addTimer(int timeout, TimerFunc callback, void* userData);
    void removeTimer(int handle);
    void addFileDescriptor(int fd, unsigned int flags, FdFunc callback, void* userData);
    void removeFileDescriptor(int fd, unsigned int flags = 0);

    void run(int maxTime = -1);
    pthread_t thread() const { return mThread; }

    // The following two functions are thread safe
    void postEvent(EventReceiver* object, Event* event);
    void exit();
    void removeEvents(EventReceiver *e);
private:
    void handlePipe();
    void sendPostedEvents();
    bool reinsertTimer(int handle, timeval* now);
    static void exitTimer(int id, void *userData)
    {
        EventLoop *loop = static_cast<EventLoop*>(userData);
        loop->removeTimer(id);
        loop->exit();
    }

private:
    int mEventPipe[2];
    bool mQuit;

    Mutex mMutex;

    struct FdData {
        unsigned int flags;
        FdFunc callback;
        void* userData;
    };
    Map<int, FdData> mFdData;

    int mNextTimerHandle;
    struct TimerData {
        int handle;
        int timeout;
        timeval when;
        TimerFunc callback;
        void* userData;
    };
    List<TimerData*> mTimerData;
    Map<int, TimerData*> mTimerByHandle;

    static bool timerLessThan(TimerData* a, TimerData* b);

    struct EventData {
        EventReceiver* receiver;
        Event* event;
    };
    std::deque<EventData> mEvents;

    static EventLoop* sInstance;

    pthread_t mThread;
};

#endif
