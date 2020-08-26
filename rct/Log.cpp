#include "Log.h"

#include <stdarg.h>
#include <stdio.h>

#ifndef _WIN32
#  include <syslog.h>
#endif
#include <mutex>

#include "Path.h"
#include "StackBuffer.h"
#include "StopWatch.h"
#include "rct/Flags.h"
#include "rct/Set.h"
#include "rct/String.h"

static Flags<LogFlag> sFlags;
static StopWatch sStart;
static Set<std::shared_ptr<LogOutput> > sOutputs;
static std::mutex sOutputsMutex;
static LogLevel sLevel = LogLevel::Error;

const LogLevel LogLevel::None(-1);
const LogLevel LogLevel::Error(0);
const LogLevel LogLevel::Warning(1);
const LogLevel LogLevel::Debug(2);
const LogLevel LogLevel::VerboseDebug(3);

static inline size_t prettyTimeSinceStarted(char *buf, size_t max)
{
    uint64_t elapsed = sStart.elapsed();
    enum {
        MS = 1,
        Second = 1000,
        Minute = Second * 60,
        Hour = Minute * 60
    };
    const int ratios[] = { Hour, Minute, Second, MS };
    int values[] = { 0, 0, 0, 0 };
    for (int i=0; i<4; ++i) {
        values[i] = elapsed / ratios[i];
        elapsed %= ratios[i];
    }
    return snprintf(buf, max, "%02d:%02d:%02d:%03d: ", values[0], values[1], values[2], values[3]);
}

static inline int writeLog(FILE *f, const char *msg, int len, Flags<LogOutput::LogFlag> flags)
{
    int ret = 0;
    if (sFlags & LogTimeStamp && flags & LogOutput::TrailingNewLine) {
        char buf[32];
        ret += prettyTimeSinceStarted(buf, sizeof(buf));
        ret += fwrite(buf, 1, ret, f);
    }
    ret += fwrite(msg, 1, len, f);
    if (flags & LogOutput::TrailingNewLine) {
        ret += fwrite("\n", 1, 1, f);
    } else {
        fflush(f);
    }
    return ret;
}
class FileOutput : public LogOutput
{
public:
    FileOutput(LogLevel level, FILE *f)
        : LogOutput(File, level), file(f)
    {
    }
    ~FileOutput()
    {
        if (file)
            fclose(file);
    }

    virtual void log(Flags<LogOutput::LogFlag> flags, const char *msg, int len) override
    {
        writeLog(file, msg, len, flags);
        fflush(file);
    }
    FILE *file;
};

class TerminalOutput : public LogOutput
{
public:
    TerminalOutput(LogLevel lvl, Flags<::LogFlag> flags)
        : LogOutput(Terminal, lvl), mReplaceableLength(0), mFlags(flags)
    {}
    virtual void log(Flags<LogOutput::LogFlag> flags, const char *msg, int len) override
    {
        FILE *f = flags & StdOut ? stdout : stderr;
        if (flags & Replaceable) {
            if (mReplaceableLength) {
                fwrite("\r", 1, 1, f);
                while (mReplaceableLength--)
                    fwrite(" ", 1, 1, f);
                fwrite("\r", 1, 1, f);
            }
            mReplaceableLength = writeLog(f, msg, len, flags);
        } else {
            if (mReplaceableLength)
                fwrite("\n", 1, 1, f);
            mReplaceableLength = 0;
            writeLog(f, msg, len, flags);
            if (mFlags & ::LogFlush) {
                fflush(f);
            }
        }
    }
private:
    int mReplaceableLength;
    Flags<::LogFlag> mFlags;
};

#ifndef _WIN32  // no syslog on windows
class SyslogOutput : public LogOutput
{
public:
    SyslogOutput(const char* ident, LogLevel lvl)
        : LogOutput(Syslog, lvl)
    {
        ::openlog(ident, LOG_CONS | LOG_NOWAIT | LOG_PID, LOG_USER);
    }
    virtual ~SyslogOutput()
    {
        ::closelog();
    }
    virtual void log(Flags<LogOutput::LogFlag>, const char *msg, int) override
    {
        ::syslog(LOG_NOTICE, "%s", msg);
    }
};
#endif

void restartTime()
{
    sStart.restart();
}

static void logHelper(LogLevel level, Flags<LogOutput::LogFlag> flags, const char *format, va_list v)
{
    if (!testLog(level))
        return;

    va_list v2;
    va_copy(v2, v);
    enum { Size = 16384 };

    StackBuffer<Size> buf(Size);
    int n = vsnprintf(buf, Size, format, v);
    if (n == -1) {
        va_end(v2);
        return;
    }

    if (n >= Size) {
        buf.resize(n + 2);
        n = vsnprintf(buf, n + 1, format, v2);
    }

    logDirect(level, buf, n, flags);
    va_end(v2);
}

void logDirect(LogLevel level, const char *msg, int len, Flags<LogOutput::LogFlag> flags)
{
    Set<std::shared_ptr<LogOutput> > logs;
    {
        std::lock_guard<std::mutex> lock(sOutputsMutex);
        logs = sOutputs;
    }
    if (logs.isEmpty()) {
        fwrite(msg, len, 1, stdout);
        if (flags & LogOutput::TrailingNewLine)
            fwrite("\n", 1, 1, stdout);
    } else {
        for (const auto &output : logs) {
            if (output->testLog(level)) {
                output->log(flags, msg, len);
            }
        }
    }
}

void log(const std::function<void(const std::shared_ptr<LogOutput> &)> &func)
{
    Set<std::shared_ptr<LogOutput> > logs;
    {
        std::lock_guard<std::mutex> lock(sOutputsMutex);
        logs = sOutputs;
    }
    for (const auto &out : logs) {
        func(out);
    }
}

void log(LogLevel level, const char *format, ...)
{
    va_list v;
    va_start(v, format);
    logHelper(level, LogOutput::DefaultFlags, format, v);
    va_end(v);
}

void log(LogLevel level, Flags<LogOutput::LogFlag> flags, const char *format, ...)
{
    va_list v;
    va_start(v, format);
    logHelper(level, flags, format, v);
    va_end(v);
}


void debug(const char *format, ...)
{
    va_list v;
    va_start(v, format);
    logHelper(LogLevel::Debug, LogOutput::DefaultFlags, format, v);
    va_end(v);
}

void verboseDebug(const char *format, ...)
{
    va_list v;
    va_start(v, format);
    logHelper(LogLevel::VerboseDebug, LogOutput::DefaultFlags, format, v);
    va_end(v);
}

void warning(const char *format, ...)
{
    va_list v;
    va_start(v, format);
    logHelper(LogLevel::Warning, LogOutput::DefaultFlags, format, v);
    va_end(v);
}

void error(const char *format, ...)
{
    va_list v;
    va_start(v, format);
    logHelper(LogLevel::Error, LogOutput::DefaultFlags, format, v);
    va_end(v);
}

bool testLog(LogLevel level)
{
    std::lock_guard<std::mutex> lock(sOutputsMutex);
    if (sOutputs.isEmpty())
        return true;
    for (const auto &output : sOutputs) {
        if (output->testLog(level))
            return true;
    }
    return false;
}

LogLevel logLevel()
{
    return sLevel;
}

bool initLogging(const char* ident, Flags<LogFlag> flags, LogLevel level,
                 const Path &file, LogLevel logFileLogLevel)
{
    (void) ident;  //unused
    if (getenv("RCT_LOG_TIME"))
        flags |= LogTimeStamp;

    sStart.start();
    sFlags = flags;
    sLevel = level;
    if (flags & LogStderr) {
        std::shared_ptr<TerminalOutput> out(new TerminalOutput(level, flags));
        out->add();
    }
#ifndef _WIN32
    if (flags & LogSyslog) {
        std::shared_ptr<SyslogOutput> out(new SyslogOutput(ident, level));
        out->add();
    }
#endif
    if (!file.isEmpty()) {
        if (!(flags & (Append|DontRotate)) && file.exists()) {
            int i = 0;
            while (true) {
                const Path rotated = String::format<64>("%s.%d", file.constData(), ++i);
                if (!rotated.exists()) {
                    if (rename(file.constData(), rotated.constData())) {
                        error() << "Couldn't rotate log file" << file << "to" << rotated << Rct::strerror();
                    }
                    break;
                }
            }
        }
        FILE *f = fopen(file.constData(), flags & Append ? "a" : "w");
        if (!f)
            return false;
        std::shared_ptr<FileOutput> out(new FileOutput(logFileLogLevel, f));
        out->add();
    }
    return true;
}

void cleanupLogging()
{
    std::lock_guard<std::mutex> lock(sOutputsMutex);
    sOutputs.clear();
}

Log::Log(String *out, Flags<LogOutput::LogFlag> flags)
{
    assert(out);
    mData.reset(new Data(out, flags));
}

Log::Log(LogLevel level, Flags<LogOutput::LogFlag> flags)
{
    if (testLog(level))
        mData.reset(new Data(level, flags));
}

Log::Log(const Log &other)
    : mData(other.mData)
{
}

Log &Log::operator=(const Log &other)
{
    mData = other.mData;
    return *this;
}

LogOutput::LogOutput(Type type, LogLevel logLevel)
    : mType(type), mLogLevel(logLevel)
{
}

LogOutput::~LogOutput()
{
}

void LogOutput::add()
{
    std::lock_guard<std::mutex> lock(sOutputsMutex);
    sOutputs.insert(shared_from_this());
}

void LogOutput::remove()
{
    std::lock_guard<std::mutex> lock(sOutputsMutex);
    sOutputs.remove(shared_from_this());
}
