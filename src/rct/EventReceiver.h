#ifndef EVENTRECEIVER_H
#define EVENTRECEIVER_H

#include <rct/EventLoop.h>
#include <rct/Event.h>
#include <rct/SignalSlot.h>

namespace Rct {
enum TimerMode {
    Repeat,
    SingleShot
};
}

class TimerEvent
{
public:
    TimerEvent() : mId(0), mInterval(0), mTimerMode(Rct::SingleShot), mUserData(0) {}
    void stop() { mTimerMode = Rct::SingleShot; }
    inline int id() const { return mId; }
    inline int interval() const { return mInterval; }
    inline Rct::TimerMode timerMode() const { return mTimerMode; }
    inline void *userData() const { return mUserData; }
private:
    int mId, mInterval;
    Rct::TimerMode mTimerMode;
    void *mUserData;
    friend class EventReceiver;
};
class EventReceiver : public enable_shared_from_this<EventReceiver>
{
public:
    EventReceiver();
    virtual ~EventReceiver();
    void postEvent(Event *event); // threadsafe
    void deleteLater();

    int startTimer(int interval, Rct::TimerMode timerMode, void *userData = 0);
    bool stopTimer(int id);
protected:
    virtual void timerEvent(TimerEvent *event);
    virtual void event(const Event *event);

private:
    void sendEvent(const Event *event);

    class DeleteLaterEvent : public Event
    {
    public:
        enum { Type = -1 };
        DeleteLaterEvent()
            : Event(Type)
        {}
    };

    Map<int, TimerEvent> mTimers;
    static void timerEventCallBack(int id, void *userData);
    friend class EventLoop;
};

class Timer
{
public:
    inline Timer()
        : mId(0), mInterval(0), mTimerMode(Rct::Repeat), mUserData(0)
    {}

    inline int start(const shared_ptr<EventReceiver> &receiver, int interval, Rct::TimerMode mode, void *userData = 0)
    {
        assert(receiver);
        assert(interval >= 0);
        stop();
        mId = receiver->startTimer(interval, mode, userData);
        mReceiver = receiver;
        return mId;
    }

    inline void stop()
    {
        shared_ptr<EventReceiver> receiver = mReceiver.lock();
        if (mId > 0 && receiver) {
            receiver->stopTimer(mId);
            mId = mInterval = -1;
            mUserData = 0;
            mTimerMode = Rct::SingleShot;
            mReceiver.reset();
        }
    }

    inline int restart()
    {
        shared_ptr<EventReceiver> receiver = mReceiver.lock();
        if (receiver && mInterval) {
            return start(receiver, mInterval, mTimerMode, mUserData);
        }
        return -1;
    }

    inline int id() const
    {
        return mId;
    }

    inline int interval() const
    {
        return mInterval;
    }
    inline Rct::TimerMode timerMode() const
    {
        return mTimerMode;
    }
    inline void *userData()
    {
        return mUserData;
    }

    shared_ptr<EventReceiver> receiver() const
    {
        return mReceiver.lock();
    }

private:
    int mId, mInterval;
    Rct::TimerMode mTimerMode;
    void *mUserData;
    weak_ptr<EventReceiver> mReceiver;
};

#endif
