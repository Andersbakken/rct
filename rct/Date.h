#ifndef Date_h
#define Date_h

#include <time.h>

class Date
{
public:
    enum Mode { Local, UTC };

    Date();
    Date(time_t time, Mode mode = UTC);

    bool isEpoch() const { return !mTime; }

    int date(Mode mode = UTC) const; // day of month, 1-31
    int day(Mode mode = UTC) const; // day of week, 0-6
    int year(Mode mode = UTC) const; // year, four digits
    int hours(Mode mode = UTC) const; // hour, 0-23
    int minutes(Mode mode = UTC) const; // minutes, 0-59
    int month(Mode mode = UTC) const; // month, 0-11
    int seconds(Mode mode = UTC) const; // seconds, 0-59. Does this need Mode?

    time_t time(Mode mode = UTC) const;
    void setTime(time_t time, Mode mode = UTC);

private:
    time_t mTime; // Stored as UTC
};

#endif
