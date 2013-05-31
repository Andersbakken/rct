#include "rct/EventReceiver.h"
EventReceiver::EventReceiver()
{}

EventReceiver::~EventReceiver()
{
    if (EventLoop *loop = EventLoop::instance())
        loop->removeEvents(this);
}

void EventReceiver::postEvent(Event *event) // threadsafe
{
    assert(EventLoop::instance());
    EventLoop::instance()->postEvent(this, event);
}

void EventReceiver::deleteLater()
{
    postEvent(new DeleteLaterEvent);
}

int EventReceiver::startTimer(int interval, TimerMode timerMode, void *userData)
{

    EventLoop *loop = EventLoop::instance();
    if (!loop)
        return -1;
    const bool isMainThread = pthread_equal(pthread_self(), loop->thread());

    MutexLocker locker;
    if (!isMainThread)
        locker.lock(&mTimerMutex);

    void *eventLoopUserData = new weak_ptr<EventReceiver>(shared_from_this());
    int id = loop->addTimer(interval, timerEventCallBack, eventLoopUserData);
    TimerEvent &ev = mTimers[id];
    ev.mId = id;
    ev.mInterval = interval;
    ev.mTimerMode = timerMode;
    ev.mUserData = userData;

    return id;
}

bool EventReceiver::stopTimer(int id)
{
    return mTimers.remove(id);
    // we need to let it fire to delete the weak_ptr
}


void EventReceiver::sendEvent(const Event *e)
{
    switch (e->type()) {
    case DeleteLaterEvent::Type:
        delete this;
        break;
    case signalslot::SignalEventBase::Type:
        static_cast<const signalslot::SignalEventBase*>(e)->send();
        break;
    default:
        event(e);
    }
}

void EventReceiver::event(const Event *)
{}

void EventReceiver::timerEvent(TimerEvent *)
{}

void EventReceiver::timerEventCallBack(int id, void *userData)
{
    weak_ptr<EventReceiver> *ptr = static_cast<weak_ptr<EventReceiver> *>(userData);
    assert(ptr);
    shared_ptr<EventReceiver> receiver = ptr->lock();
    bool remove = true;
    if (receiver) {
        Map<int, TimerEvent> events;
        Set<int> toErase;
        {
            MutexLocker locker(&receiver->mTimerMutex);
            events = receiver->mTimers;
        }
        Map<int, TimerEvent>::iterator it = events.find(id);
        if (it != events.end()) {
            receiver->timerEvent(&it->second);
            if (it->second.mTimerMode == SingleShot) {
                MutexLocker locker(&receiver->mTimerMutex);
                receiver->mTimers.erase(it->first);
            } else {
                remove = false;
            }
        }
    }
    if (remove) {
        EventLoop::instance()->removeTimer(id);
        delete ptr;
    }
}
