#ifndef Rct_h
#define Rct_h

#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <rct/String.h>
#include <rct/Path.h>
#include <rct/List.h>
#include <sys/select.h>

namespace Rct {

enum { Max_USec = 1000000 };

String shortOptions(const option *longOptions);
int readLine(FILE *f, char *buf = 0, int max = -1);
inline int fileSize(FILE *f)
{
    assert(f);
    const int pos = ftell(f);
    fseek(f, 0, SEEK_END);
    const int ret = ftell(f);
    fseek(f, pos, SEEK_SET);
    return ret;
}
bool readFile(const Path& path, String& data);
bool writeFile(const Path& path, const String& data);
String filterPreprocessor(const Path &path);
void removeDirectory(const Path &path);
int canonicalizePath(char *path, int len);
String unescape(String command);
bool startProcess(const Path &dotexe, const List<String> &dollarArgs);
void findExecutablePath(const char *argv0);
Path executablePath();
String backtrace(int maxFrames = -1);
bool gettime(timeval* time);

inline bool timevalGreaterEqualThan(const timeval* a, const timeval* b)
{
    return (a->tv_sec > b->tv_sec
            || (a->tv_sec == b->tv_sec && a->tv_usec >= b->tv_usec));
}

inline void timevalAdd(timeval* a, int diff)
{
    a->tv_sec += diff / 1000;
    a->tv_usec += (diff % 1000) * 1000;
    if (a->tv_usec >= Max_USec) {
        ++a->tv_sec;
        a->tv_usec -= Max_USec;
    }
}

inline void timevalSub(timeval* a, timeval* b)
{
    a->tv_sec -= b->tv_sec;
    a->tv_usec -= b->tv_usec;
    if (a->tv_sec < 0) {
        a->tv_sec = a->tv_usec = 0;
    } else if (a->tv_usec < 0) {
        if (--a->tv_sec < 0) {
            a->tv_sec = a->tv_usec = 0;
        } else {
            a->tv_usec += Max_USec;
        }
    }
}

inline uint64_t timevalMs(timeval* a)
{
    return (a->tv_sec * 1000LLU) + (a->tv_usec / 1000LLU);
}

inline int timevalDiff(timeval* a, timeval* b)
{
    const uint64_t ams = timevalMs(a);
    const uint64_t bms = timevalMs(b);
    return ams - bms;
}

}

#define eintrwrap(VAR, BLOCK)                   \
    do {                                        \
        VAR = BLOCK;                            \
    } while (VAR == -1 && errno == EINTR);

#endif
