#ifndef OnDestruction_h
#define OnDestruction_h

#include <functional>

class OnDestruction
{
public:
    OnDestruction(const std::function<void()> &func)
        : mFunc(func)
    {}
    ~OnDestruction()
    {
        if (mFunc)
            mFunc();
    }
private:
    std::function<void()> mFunc;
};

#endif
