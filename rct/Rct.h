#ifndef Rct_h
#define Rct_h

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <rct/List.h>
#include <rct/Path.h>
#include <rct/String.h>
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <regex>
#include <functional>
#include <vector>

#include "rct/List.h"
#include "rct/String.h"

struct option;
struct timeval;

extern char **environ;

#ifndef RCT_FALL_THROUGH
#if defined(__cplusplus) && (__cplusplus > 201402L)
#define RCT_FALL_THROUGH() /* fall through */ [[fallthrough]]
#else
#if defined(__GNUC__)
#if __GXX_ABI_VERSION >= 1011 && !defined(__ANDROID__)
#define RCT_FALL_THROUGH /* fall through */ __attribute__((fallthrough))
#else
#define RCT_FALL_THROUGH /* fall through */
#endif
#elif defined(__clang__)
#define RCT_FALL_THROUGH /* fall through */ [[clang::fallthrough]]
#else
#define RCT_FALL_THROUGH /* fall through */ struct RCT_FALL_THROUGH_STRUCT
#endif
#endif
#endif

namespace Rct
{

constexpr bool is64Bit = sizeof(void *) == 8;

template <typename T, size_t N> constexpr size_t countof(T (&)[N])
{
    return N;
}

enum { Max_USec = 1000000 };

inline size_t indexIn(const String &string, const std::regex &rx)
{
    std::cmatch match;
    if (std::regex_match(string.constData(), match, rx) && !match.empty())
        return match.position(0);
    return String::npos;
}

inline bool contains(const char *str, const std::regex &rx, std::cmatch *match = nullptr)
{
    std::cmatch null;
    std::cmatch &m = match ? *match : null;
    return std::regex_match(str, m, rx);
}

inline bool contains(const String &str, const std::regex &rx, std::cmatch *match = nullptr)
{
    return contains(str.constData(), rx, match);
}

String shortOptions(const option *longOptions);
int readLine(FILE *f, char *buf = nullptr, int max = -1);
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
template <typename Container, typename Value> inline bool addTo(Container &container, const Value &value)
{
    const int oldSize = container.size();
    container += value;
    return container.size() != oldSize;
}
bool readFile(const Path &path, String &data, mode_t *perm = nullptr);
bool readFile(FILE *f, String &data, mode_t *perm = nullptr);
bool writeFile(const Path &path, const String &data, int perm = -1);

/**
 * Finds the absolute path (including the file name) of the currently running
 * executable.
 * After calling this function, get the result by calling executablePath().
 */
void findExecutablePath(const char *argv0);

/**
 * Get the path previously calculated by findExecutablePath().
 */
Path executablePath();

String backtrace(int maxFrames = -1);
bool gettime(timeval *time);
uint64_t monoMs();
uint64_t currentTimeMs();
String currentTimeString();
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
String colorize(const String &string, AnsiColor color, size_t from = 0, size_t len = -1);
enum LookupMode { Auto, IPv4, IPv6 };
String addrLookup(const String &addr, LookupMode mode = Auto, bool *ok = nullptr);
String nameLookup(const String &name, LookupMode mode = IPv4, bool *ok = nullptr);
bool isIP(const String &addr, LookupMode mode = Auto);

inline void jsonEscape(const String &str, std::function<void(const char *, size_t)> output)
{
    output("\"", 1);
    bool hasEscaped = false;
    size_t i;
    auto put = [&output, &hasEscaped, &i, &str](const char *escaped) {
        if (!hasEscaped) {
            hasEscaped = true;
            if (i)
                output(str.constData(), i);
        }
        output(escaped, strlen(escaped));
    };
    const char *stringData = str.constData();

    const size_t length = str.size();
    for (i = 0; i < length; ++i) {
        switch (const char ch = stringData[i]) {
        case 8:
            put("\\b");
            break; // backspace
        case 12:
            put("\\f");
            break; // Form feed
        case '\n':
            put("\\n");
            break; // newline
        case '\t':
            put("\\t");
            break; // tab
        case '\r':
            put("\\r");
            break; // carriage return
        case '"':
            put("\\\"");
            break; // quote
        case '\\':
            put("\\\\");
            break; // backslash
        default:
            if (ch < 0x20 || ch == 127) { // escape non printable characters
                char buffer[7];
                snprintf(buffer, 7, "\\u%04x", ch);
                put(buffer);
                break;
            } else if (hasEscaped) {
                output(&ch, 1);
            }
            break;
        }
    }

    if (!hasEscaped)
        output(stringData, length);
    output("\"", 1);
}

inline String jsonEscape(const String &string)
{
    String ret;
    jsonEscape(string,
               std::bind(
                   static_cast<void (String::*)(const char *, size_t)>(&String::append), &ret, std::placeholders::_1, std::placeholders::_2));
    return ret;
}

inline bool timevalGreaterEqualThan(const timeval *a, const timeval *b)
{
    return (a->tv_sec > b->tv_sec || (a->tv_sec == b->tv_sec && a->tv_usec >= b->tv_usec));
}

inline void timevalAdd(timeval *a, int diff)
{
    a->tv_sec += diff / 1000;
    a->tv_usec += (diff % 1000) * 1000;
    if (a->tv_usec >= Max_USec) {
        ++a->tv_sec;
        a->tv_usec -= Max_USec;
    }
}

inline void timevalSub(timeval *a, timeval *b)
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

inline uint64_t timevalMs(timeval *a)
{
    return (a->tv_sec * 1000) + (a->tv_usec / 1000);
}

inline int timevalDiff(timeval *a, timeval *b)
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

inline List<String> environment()
{
    List<String> environment;
    char **env = environ;
    while (*env) {
        environment.push_back(*env);
        ++env;
    }
    return environment;
}

static inline bool wildCmp(const char *wild, const char *string, String::CaseSensitivity cs = String::CaseSensitive)
{
    // Written by Jack Handy - Found here: http://www.codeproject.com/Articles/1088/Wildcard-string-compare-globbing
    const char *cp = nullptr, *mp = nullptr;

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
            cp = string + 1;
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

#ifndef RCT_FALL_THROUGH
#if defined(__clang__)
#define RCT_FALL_THROUGH /* fall through */ struct RCT_FALL_THROUGH_STRUCT
#elif defined(__GNUC__) && __GXX_ABI_VERSION >= 1011
#define RCT_FALL_THROUGH /* fall through */ __attribute__((fallthrough))
#else
#define RCT_FALL_THROUGH /* fall through */ struct RCT_FALL_THROUGH_STRUCT
#endif
#endif
#endif
