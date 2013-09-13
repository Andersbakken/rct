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

    uint64_t start()
    {
        return (mStart = current(mPrecision));
    }

    uint64_t startTime() const
    {
        return mStart;
    }

    static uint64_t current(Precision prec)
    {
        timeval t;
        Rct::gettime(&t); // monotonic
        if (prec == Millisecond) {
            return (t.tv_sec * 1000) + (t.tv_usec / 1000);
        } else {
            return (t.tv_sec * 1000000) + t.tv_usec;
        }
    }

    uint64_t elapsed() const
    {
        return current(mPrecision) - mStart;
    }

    uint64_t restart()
    {
        const long int cur = current(mPrecision);
        const long int ret = cur - mStart;
        mStart = cur;
        return ret;
    }
    Precision precision() const { return mPrecision; }
private:
    const Precision mPrecision;
    uint64_t mStart;
};
#endif

