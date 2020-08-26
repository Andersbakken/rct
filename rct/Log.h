#ifndef Log_h
#define Log_h

#include <assert.h>
#include <cxxabi.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <rct/List.h>
#include <rct/Map.h>
#include <rct/Path.h>
#include <rct/Set.h>
#include <rct/String.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <climits>
#include <memory>
#include <sstream>
#include <functional>
#include <string>
#include <typeinfo>
#include <utility>

#include "Flags.h"
#include "Hash.h"
#include "rct/Path.h"

class LogLevel
{
public:
    explicit LogLevel(int val)
        : mValue(val)
    {}

    // operator int() const { return mValue; }
    bool operator==(LogLevel other) const { return mValue == other.mValue; }
    bool operator!=(LogLevel other) const { return mValue != other.mValue; }
    bool operator<(LogLevel other) const { return mValue < other.mValue; }
    bool operator<=(LogLevel other) const { return mValue <= other.mValue; }
    bool operator>(LogLevel other) const { return mValue > other.mValue; }
    bool operator>=(LogLevel other) const { return mValue >= other.mValue; }

    LogLevel &operator++() { ++mValue; return *this; }
    LogLevel operator++(int) { return LogLevel(mValue++); }
    LogLevel &operator--() { --mValue; return *this; }
    LogLevel operator--(int) { return LogLevel(mValue--); }

    int toInt() const { return mValue; }

    static const LogLevel None;
    static const LogLevel Error;
    static const LogLevel Warning;
    static const LogLevel Debug;
    static const LogLevel VerboseDebug;
private:
    int mValue;
};

class LogOutput : public std::enable_shared_from_this<LogOutput>
{
public:
    enum Type {
        Terminal,
        File,
        Syslog,
        Custom
    };
    LogOutput(Type type, LogLevel logLevel);
    virtual ~LogOutput();

    void add();
    void remove();

    Type type() const { return mType; }

    virtual bool testLog(LogLevel level) const
    {
        return level >= LogLevel::Error && level <= mLogLevel;
    }
    enum LogFlag {
        None = 0x0,
        TrailingNewLine = 0x1,
        NoTypename = 0x2,
        Replaceable = 0x4,
        StdOut = 0x8,
        DefaultFlags = TrailingNewLine
    };
    virtual void log(Flags<LogFlag> /*flags*/, const char */*msg*/, int /*len*/) { }
    void log(const String &msg) { log(Flags<LogFlag>(DefaultFlags), msg.constData(), msg.length()); }
    template <int StaticBufSize = 256>
    void vlog(const char *format, ...) RCT_PRINTF_WARNING(2, 3);
    LogLevel logLevel() const { return mLogLevel; }
private:
    const Type mType;
    LogLevel mLogLevel;
};

template <int StaticBufSize>
inline void LogOutput::vlog(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    log(String::format<StaticBufSize>(format, args));
    va_end(args);
}

RCT_FLAGS(LogOutput::LogFlag);

void log(LogLevel level, Flags<LogOutput::LogFlag> flags, const char *format, ...) RCT_PRINTF_WARNING(3, 4);
void log(LogLevel level, const char *format, ...) RCT_PRINTF_WARNING(2, 3);
void debug(const char *format, ...) RCT_PRINTF_WARNING(1, 2);
void verboseDebug(const char *format, ...) RCT_PRINTF_WARNING(1, 2);
void warning(const char *format, ...) RCT_PRINTF_WARNING(1, 2);
void error(const char *format, ...) RCT_PRINTF_WARNING(1, 2);
void logDirect(LogLevel level, const char *str, int length, Flags<LogOutput::LogFlag> flags = LogOutput::DefaultFlags);
inline void logDirect(LogLevel level, const String &out, Flags<LogOutput::LogFlag> flags = LogOutput::DefaultFlags)
{
    return logDirect(level, out.constData(), out.size(), flags);
}
void log(const std::function<void(const std::shared_ptr<LogOutput> &)> &func);

bool testLog(LogLevel level);

enum LogFlag {
    Append = 0x01,
    DontRotate = 0x02,
    LogStderr = 0x04,
#ifndef _WIN32
    LogSyslog = 0x08,
#endif
    LogTimeStamp = 0x10,
    LogFlush = 0x20
};
RCT_FLAGS(LogFlag);

bool initLogging(const char* ident,
                 Flags<LogFlag> flags = LogStderr,
                 LogLevel logLevel = LogLevel::Error,
                 const Path &logFile = Path(),
                 LogLevel logFileLogLevel = LogLevel::VerboseDebug);
void cleanupLogging();
LogLevel logLevel();
void restartTime();
class Log
{
public:
    Log(String *out, Flags<LogOutput::LogFlag> flags = LogOutput::DefaultFlags);
    Log(LogLevel level = LogLevel::Error, Flags<LogOutput::LogFlag> flags = LogOutput::DefaultFlags);
    Log(const Log &other);
    Log &operator=(const Log &other);
#if defined(OS_Darwin)
#ifndef __i386__
    Log operator<<(long number) { return addStringStream(number); }
#endif
    Log operator<<(size_t number) { return addStringStream(number); }
#elif (ULONG_MAX) != (UINT_MAX)
    Log operator<<(uint64_t number) { return addStringStream(number); }
    Log operator<<(int64_t number) { return addStringStream(number); }
#endif
#if defined(__i386__)
    Log operator<<(long number) { return addStringStream(number); }
#endif
    Log operator<<(unsigned long long number) { return addStringStream(number); }
    Log operator<<(long long number) { return addStringStream(number); }
#ifdef _WIN32
    /// at least on mingw64, this is required for the DWORD data type.
    /// which is strange actually, because DWORD is an unsigned 32-bit integer
    /// just like uint32_t.
    Log operator<<(unsigned long number) { return addStringStream(number); }
#endif
    Log operator<<(uint32_t number) { return addStringStream(number); }
    Log operator<<(int32_t number) { return addStringStream(number); }
    Log operator<<(uint16_t number) { return addStringStream(number); }
    Log operator<<(int16_t number) { return addStringStream(number); }
    Log operator<<(uint8_t number) { return addStringStream<uint16_t>(number); }
    Log operator<<(int8_t number) { return addStringStream(number); }
    Log operator<<(float number) { return addStringStream(number); }
    Log operator<<(double number) { return addStringStream(number); }
    Log operator<<(long double number) { return addStringStream(number); }
    Log operator<<(char ch) { return write(&ch, 1); }
    Log operator<<(bool b) { return write(b ? "true" : "false"); }
    Log operator<<(void *ptr)
    {
        char buf[16];
        const int w = snprintf(buf, sizeof(buf), "%p", ptr);
        return write(buf, w);
    }
    Log operator<<(const char *string) { return write(string); }
    Log write(const char *data, int len = -1)
    {
        if (data && mData) {
            if (len == -1)
                len = strlen(data);
            assert(len >= 0);
            if (len) {
                String &str = mData->outPtr ? *mData->outPtr : mData->out;
                const int outLength = str.size();
                if (mData->disableSpacingOverride) {
                    --mData->disableSpacingOverride;
                    str.resize(outLength + len);
                    memcpy(str.data() + outLength, data, len);
                } else if (mData->spacing && outLength && !isspace(str.at(str.size() - 1)) && !isspace(*data)) {
                    str.resize(outLength + len + 1);
                    str[outLength] = ' ';
                    memcpy(str.data() + outLength + 1, data, len);
                } else {
                    str.resize(outLength + len);
                    memcpy(str.data() + outLength, data, len);
                }
            }
        }
        return *this;
    }
    template <int StaticBufSize = 256>
    Log log(const char *format, ...) RCT_PRINTF_WARNING(2, 3);
    void disableNextSpacing()
    {
        if (mData)
            ++mData->disableSpacingOverride;
    }
    bool setSpacing(bool on)
    {
        if (mData) {
            const bool ret = mData->spacing;
            mData->spacing = on;
            return ret;
        }
        return false;
    }

    bool spacing() const
    {
        return mData && mData->spacing;
    }
    template <typename T>
    static String toString(const T &t)
    {
        String ret;
        {
            Log l(&ret);
            l << t;
        }
        return ret;
    }
    Flags<LogOutput::LogFlag> flags() const
    {
        if (mData)
            return mData->flags;
        return Flags<LogOutput::LogFlag>();
    }
private:
    template <typename T> Log addStringStream(T t)
    {
        if (mData) {
            std::ostringstream str;
            str << t;
            const std::string string = str.str();
            return write(string.data(), string.size());
        }
        return *this;
    }
    class Data
    {
    public:
        Data(String *string, Flags<LogOutput::LogFlag> f)
            : outPtr(string), level(LogLevel::None), spacing(true), disableSpacingOverride(0), flags(f)
        {}
        Data(LogLevel lvl, Flags<LogOutput::LogFlag> f)
            : outPtr(nullptr), level(lvl), spacing(true), disableSpacingOverride(0), flags(f)
        {
        }
        ~Data()
        {
            if (!out.isEmpty()) {
                logDirect(level, out, flags);
            }
        }

        String *outPtr;
        const LogLevel level;
        String out;
        bool spacing;
        int disableSpacingOverride;
        Flags<LogOutput::LogFlag> flags;
    };

    std::shared_ptr<Data> mData;
};

template <int StaticBufSize>
inline Log Log::log(const char *format, ...)
{
    if (mData) {
        va_list args;
        va_start(args, format);
        const String str = String::format<StaticBufSize>(format, args);
        write(str.constData(), str.size());
        va_end(args);
    }
    return *this;
}

template <typename T> inline String typeName()
{
#ifdef __GXX_RTTI
    const char *name = typeid(T).name();
    char *ret = abi::__cxa_demangle(name, nullptr, nullptr, nullptr);
    String ba;
    if (ret) {
        ba = ret;
        free(ret);
    }
    return ba;
#else
    return String();
#endif
}

template <typename T>
inline Log operator<<(Log stream, const std::shared_ptr<T> &ptr)
{
    if (!(stream.flags() & LogOutput::NoTypename))
        stream << ("std::shared_ptr<" + typeName<T>() + ">");
    stream << ptr.get();
    return stream;
}

template <typename T>
inline Log operator<<(Log stream, const List<T> &list)
{
    bool old;
    if (!(stream.flags() & LogOutput::NoTypename)) {
        stream << "List<";
        old = stream.setSpacing(false);
        stream << typeName<T>() << ">(";
    } else {
        old = stream.setSpacing(false);
    }
    bool first = true;
    for (typename List<T>::const_iterator it = list.begin(); it != list.end(); ++it) {
        if (first) {
            stream.disableNextSpacing();
            first = false;
        } else {
            stream << ", ";
        }
        stream.setSpacing(old);
        stream << *it;
        old = stream.setSpacing(false);

    }
    if (!(stream.flags() & LogOutput::NoTypename))
        stream << ")";
    stream.setSpacing(old);
    return stream;
}

template <typename T1, typename T2>
inline Log operator<<(Log stream, const std::pair<T1, T2> &pair)
{
    bool old;
    if (!(stream.flags() & LogOutput::NoTypename)) {
        stream << "pair<";
        old = stream.setSpacing(false);
        stream << typeName<T1>() << ", " << typeName<T2>() << ">(";
    } else {
        old = stream.setSpacing(false);
    }
    stream << pair.first << ", " << pair.second;
    if (!(stream.flags() & LogOutput::NoTypename))
        stream << ")";
    stream.setSpacing(old);
    return stream;
}

template <typename T>
inline Log operator<<(Log stream, const Set<T> &list)
{
    bool old;
    if (!(stream.flags() & LogOutput::NoTypename)) {
        stream << "Set<";
        old = stream.setSpacing(false);
        stream << typeName<T>() << ">(";
    } else {
        old = stream.setSpacing(false);
    }
    bool first = true;
    for (typename Set<T>::const_iterator it = list.begin(); it != list.end(); ++it) {
        if (first) {
            stream.disableNextSpacing();
            first = false;
        } else {
            stream << ", ";
        }
        stream.setSpacing(old);
        stream << *it;
        old = stream.setSpacing(false);

    }
    if (!(stream.flags() & LogOutput::NoTypename))
        stream << ")";
    stream.setSpacing(old);
    return stream;
}

template <typename Key, typename Value>
inline Log operator<<(Log stream, const Map<Key, Value> &map)
{
    bool old;
    if (!(stream.flags() & LogOutput::NoTypename)) {
        stream << "Map<";
        old = stream.setSpacing(false);
        stream << typeName<Key>() << ", " << typeName<Value>() << ">(";
    } else {
        old = stream.setSpacing(false);
    }
    bool first = true;
    for (typename Map<Key, Value>::const_iterator it = map.begin(); it != map.end(); ++it) {
        if (first) {
            stream.disableNextSpacing();
            first = false;
        } else {
            stream << ", ";
        }
        const Key &key = it->first;
        const Value &value = it->second;
        stream.setSpacing(old);
        stream << key;
        old = stream.setSpacing(false);
        stream << ": ";
        stream.setSpacing(old);
        stream << value;
        old = stream.setSpacing(false);
    }
    if (!(stream.flags() & LogOutput::NoTypename))
        stream << ")";
    stream.setSpacing(old);
    return stream;
}

template <typename Key, typename Value>
inline Log operator<<(Log stream, const Hash<Key, Value> &map)
{
    bool old;
    if (!(stream.flags() & LogOutput::NoTypename)) {
        stream << "Hash<";
        old = stream.setSpacing(false);
        stream << typeName<Key>() << ", " << typeName<Value>() << ">(";
    } else {
        old = stream.setSpacing(false);
    }
    bool first = true;
    for (typename Hash<Key, Value>::const_iterator it = map.begin(); it != map.end(); ++it) {
        if (first) {
            stream.disableNextSpacing();
            first = false;
        } else {
            stream << ", ";
        }
        const Key &key = it->first;
        const Value &value = it->second;
        stream.setSpacing(old);
        stream << key;
        old = stream.setSpacing(false);
        stream << ": ";
        stream.setSpacing(old);
        stream << value;
        old = stream.setSpacing(false);
    }
    if (!(stream.flags() & LogOutput::NoTypename))
        stream << ")";
    stream.setSpacing(old);
    return stream;
}

template <typename T>
inline Log operator<<(Log log, Flags<T> f)
{
    log << f.toString();
    return log;
}

inline Log operator<<(Log stream, const String &byteArray)
{
    stream.write(byteArray.constData(), byteArray.size());
    return stream;
}

template <typename T>
String &operator<<(String &str, const T &t)
{
    Log l(&str);
    l.setSpacing(false);
    l << t;
    return str;
}

static inline Log error()
{
    return Log(LogLevel::Error);
}

static inline Log warning()
{
    return Log(LogLevel::Warning);
}

static inline Log debug()
{
    return Log(LogLevel::Debug);
}

static inline Log verboseDebug()
{
    return Log(LogLevel::VerboseDebug);
}

#endif
