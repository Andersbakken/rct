#ifndef Rct_h
#define Rct_h

#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <rct/String.h>
#include <rct/Path.h>
#include <rct/List.h>
#include <sys/select.h>
#include <regex>

namespace Rct {

constexpr bool is64Bit = sizeof(void*) == 8;

template <typename T, size_t N>
constexpr size_t countof(T(&)[N])
{
    return N;
}

enum { Max_USec = 1000000 };

inline int indexIn(const String &string, const std::regex &rx)
{
    std::cmatch match;
    if (std::regex_match(string.constData(), match, rx) && !match.empty())
        return match.position(0);
    return -1;
}

inline bool contains(const char *str, const std::regex &rx, std::cmatch *match = 0)
{
    std::cmatch null;
    std::cmatch &m = match ? *match : null;
    return std::regex_match(str, m, rx);
}

inline bool contains(const String &str, const std::regex &rx, std::cmatch *match = 0)
{
    return contains(str.constData(), rx, match);
}


String shortOptions(const option *longOptions);
int readLine(FILE *f, char *buf = 0, int max = -1);
String readAll(FILE *f, int max = -1);
inline int fileSize(FILE *f)
{
    assert(f);
    const int pos = ftell(f);
    fseek(f, 0, SEEK_END);
    const int ret = ftell(f);
    fseek(f, pos, SEEK_SET);
    return ret;
}
template <typename Container, typename Value>
inline bool addTo(Container &container, const Value &value)
{
    const int oldSize = container.size();
    container += value;
    return container.size() != oldSize;
}
bool readFile(const Path& path, String &data, mode_t *perm = 0 );
bool readFile(FILE *f, String &data, mode_t *perm = 0);
bool writeFile(const Path& path, const String& data, int perm = -1);
void removeDirectory(const Path &path);
String unescape(String command);
void findExecutablePath(const char *argv0);
Path executablePath();
String backtrace(int maxFrames = -1);
bool gettime(timeval* time);
uint64_t monoMs();
uint64_t currentTimeMs();
String hostName();

enum AnsiColor {
    AnsiColor_Default,
    AnsiColor_Black,
    AnsiColor_Red,
    AnsiColor_Green,
    AnsiColor_Yellow,
    AnsiColor_Blue,
    AnsiColor_Magenta,
    AnsiColor_Cyan,
    AnsiColor_White,
    AnsiColor_BrightDefault,
    AnsiColor_BrightBlack,
    AnsiColor_BrightRed,
    AnsiColor_BrightGreen,
    AnsiColor_BrightYellow,
    AnsiColor_BrightBlue,
    AnsiColor_BrightMagenta,
    AnsiColor_BrightCyan,
    AnsiColor_BrightWhite
};
String colorize(const String &string, AnsiColor color, int from = 0, int len = -1);
enum LookupMode { Auto, IPv4, IPv6 };
String addrLookup(const String& addr, LookupMode mode = Auto, bool *ok = 0);
String nameLookup(const String& name, LookupMode mode = IPv4, bool *ok = 0);
bool isIP(const String& addr, LookupMode mode = Auto);

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
    return (a->tv_sec * 1000) + (a->tv_usec / 1000);
}

inline int timevalDiff(timeval* a, timeval* b)
{
    const uint64_t ams = timevalMs(a);
    const uint64_t bms = timevalMs(b);
    return ams - bms;
}

inline List<Path> pathEnvironment()
{
    const char *path = getenv("PATH");
    if (path)
        return String(path).split(':', String::SkipEmpty);
    return List<Path>();
}

static inline bool wildCmp(const char *wild, const char *string, String::CaseSensitivity cs = String::CaseSensitive)
{
    // Written by Jack Handy - Found here: http://www.codeproject.com/Articles/1088/Wildcard-string-compare-globbing
    const char *cp = 0, *mp = 0;

    while (*string && *wild != '*') {
        if (*wild != '?' && *wild != *string && (cs == String::CaseSensitive || tolower(*wild) != tolower(*string))) {
            return false;
        }
        wild++;
        string++;
    }

    while (*string) {
        if (*wild == '*') {
            if (!*++wild) {
                return true;
            }
            mp = wild;
            cp = string+1;
        } else if (*wild == '?' || *wild == *string || (cs == String::CaseInsensitive && tolower(*wild) == tolower(*string))) {
            wild++;
            string++;
        } else {
            wild = mp;
            string = cp++;
        }
    }

    while (*wild == '*') {
        wild++;
    }
    return !*wild;
}

String strerror(int error = errno);
}

#define eintrwrap(VAR, BLOCK)                   \
    do {                                        \
        VAR = BLOCK;                            \
    } while (VAR == -1 && errno == EINTR)

#endif
