#include "Date.h"
#include <mutex>

static std::once_flag tzFlag;

Date::Date(time_t time, Mode mode)
{
    std::call_once(tzFlag, []() {
            tzset();
            tzsetwall();
        });
    if (mode == UTC)
        mTime = time;
    else
        mTime = time + timezone;
}

static struct tm* modetime(const time_t& clock, struct tm* result, Date::Mode mode)
{
    if (mode == Date::UTC)
        return gmtime_r(&clock, result);
    return localtime_r(&clock, result);
}

int Date::date(Mode mode) const
{
    struct tm result;
    return modetime(mTime, &result, mode)->tm_mday;
}

int Date::day(Mode mode) const
{
    struct tm result;
    return modetime(mTime, &result, mode)->tm_wday;
}

int Date::year(Mode mode) const
{
    struct tm result;
    return modetime(mTime, &result, mode)->tm_year + 1900;
}

int Date::hours(Mode mode) const
{
    struct tm result;
    return modetime(mTime, &result, mode)->tm_hour;
}

int Date::minutes(Mode mode) const
{
    struct tm result;
    return modetime(mTime, &result, mode)->tm_min;
}

int Date::month(Mode mode) const
{
    struct tm result;
    return modetime(mTime, &result, mode)->tm_mon;
}

int Date::seconds(Mode mode) const
{
    struct tm result;
    return modetime(mTime, &result, mode)->tm_sec;
}

time_t Date::time(Mode mode) const
{
    if (mode == UTC)
        return mTime;
    struct tm local;
    return mktime(localtime_r(&mTime, &local));
}
