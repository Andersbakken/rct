#include "Timer.h"

#include <map>

#include "EventLoop.h"
#include "rct/SignalSlot.h"

Timer::Timer()
    : timerId(0)
{
}

Timer::Timer(int interval, int flags)
    : timerId(0)
{
    restart(interval, flags);
}

Timer::~Timer()
{
    stop();
}

void Timer::restart(int interval, int flags, const std::shared_ptr<EventLoop> &l)
{
    std::shared_ptr<EventLoop> loop = l ? l : EventLoop::eventLoop();
    if (loop) {
        // ### this is a bit inefficient, should revisit
        if (timerId)
            loop->unregisterTimer(timerId);
        timerId = loop->registerTimer(std::bind(&Timer::timerFired, this, std::placeholders::_1),
                                      interval, flags);
    }
}

void Timer::stop()
{
    if (timerId) {
        if (std::shared_ptr<EventLoop> loop = EventLoop::eventLoop()) {
            loop->unregisterTimer(timerId);
        }
        timerId = 0;
    }
}

void Timer::timerFired(int /*id*/)
{
    signalTimeout(this);
}
