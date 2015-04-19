#include "Log.h"
#include "Path.h"
#include <mutex>
#include <stdio.h>
#include <errno.h>
#include "StopWatch.h"
#include <stdarg.h>
#include <syslog.h>

static unsigned int sFlags = 0;
static StopWatch sStart;
static Set<std::shared_ptr<LogOutput> > sOutputs;
static std::mutex sOutputsMutex;
static int sLevel = 0;
static const bool sTimedLogs = getenv("RCT_LOG_TIME");

class FileOutput : public LogOutput
{
public:
    FileOutput(FILE *f)
        : LogOutput(INT_MAX), file(f)
    {
    }
    ~FileOutput()
    {
        if (file)
            fclose(file);
    }

    virtual void log(const char *msg, int len) override
    {
        if (sTimedLogs) {
            const String time = String::formatTime(::time(0), String::Time);
            fwrite(time.constData(), time.size(), 1, file);
        }
        fwrite(msg, len, 1, file);
        fwrite("\n", 1, 1, file);
        fflush(file);
    }
    FILE *file;
};

class StderrOutput : public LogOutput
{
public:
    StderrOutput(int lvl)
        : LogOutput(lvl)
    {}
    virtual void log(const char *msg, int) override
    {
        if (sTimedLogs) {
            fprintf(stderr, "%s %s\n", String::formatTime(time(0), String::Time).constData(), msg);
        } else {
            fprintf(stderr, "%s\n", msg);
        }
    }
};

class SyslogOutput : public LogOutput
{
public:
    SyslogOutput(const char* ident, int lvl)
        : LogOutput(lvl)
    {
        ::openlog(ident, LOG_CONS | LOG_NOWAIT | LOG_PID, LOG_USER);
    }
    virtual ~SyslogOutput()
    {
        ::closelog();
    }
    virtual void log(const char *msg, int) override
    {
        ::syslog(LOG_NOTICE, "%s", msg);
    }
};

void restartTime()
{
    sStart.restart();
}

#if 0
static inline String prettyTimeSinceStarted()
{
    uint64_t elapsed = sStart.elapsed();
    char buf[128];
    enum { MS = 1,
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
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d:%03d: ", values[0], values[1], values[2], values[3]);
    return buf;
}
#endif

static void log(int level, const char *format, va_list v)
{
    if (!testLog(level))
        return;

    va_list v2;
    va_copy(v2, v);
    enum { Size = 16384 };
    char buf[Size];
    char *msg = buf;
    int n = vsnprintf(msg, Size, format, v);
    if (n == -1) {
        va_end(v2);
        return;
    }

    if (n >= Size) {
        msg = new char[n + 2];
        n = vsnprintf(msg, n + 1, format, v2);
    }

    logDirect(level, msg, n);
    if (msg != buf)
        delete []msg;
    va_end(v2);
}

void logDirect(int level, const char *msg, int len)
{
    Set<std::shared_ptr<LogOutput> > logs;
    {
        std::lock_guard<std::mutex> lock(sOutputsMutex);
        logs = sOutputs;
    }
    if (logs.isEmpty()) {
        fwrite(msg, len, 1, stdout);
        fwrite("\n", 1, 1, stdout);
    } else {
        for (const auto &output : logs) {
            if (output->testLog(level)) {
                output->log(msg, len);
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

void log(int level, const char *format, ...)
{
    va_list v;
    va_start(v, format);
    log(level, format, v);
    va_end(v);
}

void debug(const char *format, ...)
{
    va_list v;
    va_start(v, format);
    log(Debug, format, v);
    va_end(v);
}

void verboseDebug(const char *format, ...)
{
    va_list v;
    va_start(v, format);
    log(VerboseDebug, format, v);
    va_end(v);
}

void warning(const char *format, ...)
{
    va_list v;
    va_start(v, format);
    log(Warning, format, v);
    va_end(v);
}

void error(const char *format, ...)
{
    va_list v;
    va_start(v, format);
    log(Error, format, v);
    va_end(v);
}

bool testLog(int level)
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

int logLevel()
{
    return sLevel;
}

bool initLogging(const char* ident, int mode, int level, const Path &file, unsigned int flags)
{
    sStart.start();
    sFlags = flags;
    sLevel = level;
    if (mode & LogStderr) {
        std::shared_ptr<StderrOutput> out(new StderrOutput(level));
        out->add();
    }
    if (mode & LogSyslog) {
        std::shared_ptr<SyslogOutput> out(new SyslogOutput(ident, level));
        out->add();
    }
    if (!file.isEmpty()) {
        if (!(flags & (Log::Append|Log::DontRotate)) && file.exists()) {
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
        FILE *f = fopen(file.constData(), flags & Log::Append ? "a" : "w");
        if (!f)
            return false;
        std::shared_ptr<FileOutput> out(new FileOutput(f));
        out->add();
    }
    return true;
}

void cleanupLogging()
{
    std::lock_guard<std::mutex> lock(sOutputsMutex);
    sOutputs.clear();
}

Log::Log(String *out)
{
    assert(out);
    mData.reset(new Data(out));
}

Log::Log(int level)
{
    if (testLog(level))
        mData.reset(new Data(level));
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

LogOutput::LogOutput(int logLevel)
    : mLogLevel(logLevel)
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
