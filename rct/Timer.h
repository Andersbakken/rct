#ifndef TIMER_H
#define TIMER_H

#include <rct/SignalSlot.h>
#include <memory>
#include <functional>

class EventLoop;

class Timer
{
public:
    enum { SingleShot = 0x1 };

    Timer();
    Timer(int interval, int flags = 0);
    ~Timer();

    void restart(int interval, int flags = 0, const std::shared_ptr<EventLoop> &eventLoop = std::shared_ptr<EventLoop>());
    void stop();

    Signal<std::function<void(Timer*)> >& timeout() { return signalTimeout; }

    bool isRunning() const { return timerId; }
    int id() const { return timerId; }

private:
    void timerFired(int id);

private:
    int timerId;
    Signal<std::function<void(Timer*)> > signalTimeout;
};

#endif
