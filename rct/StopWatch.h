#ifndef StopWatch_h
#define StopWatch_h

#include <stdint.h>
#include <sys/time.h>

#include <rct/Rct.h>

class StopWatch
{
public:
    enum Precision {
        Millisecond,
        Microsecond
    };
    StopWatch(Precision prec = Millisecond)
        : mPrecision(prec), mStart(current(prec))
    {
    }

    unsigned long long start()
    {
        return (mStart = current(mPrecision));
    }

    unsigned long long startTime() const
    {
        return mStart;
    }

    static unsigned long long current(Precision prec)
    {
        timeval t;
        Rct::gettime(&t); // monotonic
        if (prec == Millisecond) {
            return (t.tv_sec * 1000) + (t.tv_usec / 1000);
        } else {
            return (t.tv_sec * 1000000) + t.tv_usec;
        }
    }

    unsigned long long elapsed() const
    {
        return current(mPrecision) - mStart;
    }

    unsigned long long restart()
    {
        const long int cur = current(mPrecision);
        const long int ret = cur - mStart;
        mStart = cur;
        return ret;
    }
    Precision precision() const { return mPrecision; }
private:
    const Precision mPrecision;
    unsigned long long mStart;
};
#endif
