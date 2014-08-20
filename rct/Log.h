#ifndef Log_h
#define Log_h

#include <rct/String.h>
#include <rct/List.h>
#include <rct/Map.h>
#include <rct/Hash.h>
#include <rct/Path.h>
#include <rct/Set.h>
#include <assert.h>
#include <cxxabi.h>
#include <sstream>
#include <memory>

class Path;

enum LogLevel {
    Error = 0,
    Warning = 1,
    Debug = 2,
    VerboseDebug = 3
};

class LogOutput
{
public:
    LogOutput(int logLevel);
    virtual ~LogOutput();

    virtual bool testLog(int level) const
    {
        return level >= 0 && level <= mLogLevel;
    }
    virtual void log(const char */*msg*/, int /*len*/) { }

    int logLevel() const { return mLogLevel; }

private:
    int mLogLevel;
};

#ifdef __GNUC__
void log(int level, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
void debug(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
void verboseDebug(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
void warning(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
void error(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
#else
void log(int level, const char *format, ...);
void debug(const char *format, ...);
void verboseDebug(const char *format, ...);
void warning(const char *format, ...);
void error(const char *format, ...);
#endif
void logDirect(int level, const String &out);

bool testLog(int level);
enum { LogStderr = 0x1, LogSyslog = 0x2 };
bool initLogging(const char* ident, int mode = LogStderr, int logLevel = Error, const Path &logFile = Path(), unsigned flags = 0);
void cleanupLogging();
int logLevel();
void restartTime();
class Log
{
public:
    enum Flag {
        Append = 0x1,
        DontRotate = 0x2
    };

    Log(String *out);
    Log(int level = 0);
    Log(const Log &other);
    Log &operator=(const Log &other);
#ifdef OS_Darwin
    Log operator<<(long number) { return addStringStream(number); }
    Log operator<<(size_t number) { return addStringStream(number); }
#endif
    Log operator<<(uint64_t number) { return addStringStream(number); }
    Log operator<<(int64_t number) { return addStringStream(number); }
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
    Log operator<<(bool boolean) { return write(boolean ? "true" : "false"); }
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
        Data(String *string)
            : outPtr(string), level(-1), spacing(true), disableSpacingOverride(0)
        {}
        Data(int lvl)
            : outPtr(0), level(lvl), spacing(true), disableSpacingOverride(0)
        {
        }
        ~Data()
        {
            if (!out.isEmpty()) {
                logDirect(level, out);
            }
        }

        String *outPtr;
        const int level;
        String out;
        bool spacing;
        int disableSpacingOverride;
    };

    std::shared_ptr<Data> mData;
};

template <typename T> inline String typeName()
{
#ifdef __GXX_RTTI
    const char *name = typeid(T).name();
    char *ret = abi::__cxa_demangle(name, 0, 0, 0);
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
    stream << ("std::shared_ptr<" + typeName<T>() + ">") << ptr.get();
    return stream;
}

template <typename T>
inline Log operator<<(Log stream, const List<T> &list)
{
    stream << "List<";
    bool old = stream.setSpacing(false);
    stream << typeName<T>() << ">(";
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
    stream << ")";
    stream.setSpacing(old);
    return stream;
}

template <typename T1, typename T2>
inline Log operator<<(Log stream, const std::pair<T1, T2> &pair)
{
    stream << "pair<";
    const bool old = stream.setSpacing(false);
    stream << typeName<T1>() << ", " << typeName<T2>() << ">(";
    stream << pair.first << ", " << pair.second << ")";
    stream.setSpacing(old);
    return stream;
}

template <typename T>
inline Log operator<<(Log stream, const Set<T> &list)
{
    stream << "Set<";
    bool old = stream.setSpacing(false);
    stream << typeName<T>() << ">(";
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
    stream << ")";
    stream.setSpacing(old);
    return stream;
}

template <typename Key, typename Value>
inline Log operator<<(Log stream, const Map<Key, Value> &map)
{
    stream << "Map<";
    bool old = stream.setSpacing(false);
    stream << typeName<Key>() << ", " << typeName<Value>() << ">(";
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
    stream << ")";
    stream.setSpacing(old);
    return stream;
}

template <typename Key, typename Value>
inline Log operator<<(Log stream, const Hash<Key, Value> &map)
{
    stream << "Hash<";
    bool old = stream.setSpacing(false);
    stream << typeName<Key>() << ", " << typeName<Value>() << ">(";
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
    stream << ")";
    stream.setSpacing(old);
    return stream;
}

inline Log operator<<(Log stream, const String &byteArray)
{
    stream.write(byteArray.constData(), byteArray.size());
    return stream;
}

template <typename T>
String &operator<<(String &str, const T &t)
{
    Log(&str) << t;
    return str;
}

static inline Log error()
{
    return Log(Error);
}

static inline Log warning()
{
    return Log(Warning);
}

static inline Log debug()
{
    return Log(Debug);
}

static inline Log verboseDebug()
{
    return Log(VerboseDebug);
}

#endif
