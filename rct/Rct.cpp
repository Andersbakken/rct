#include "rct/Rct.h"
#include "rct/Log.h"
#include "rct-config.h"
#include <sys/types.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/fcntl.h>
#ifdef OS_Darwin
# include <mach-o/dyld.h>
#elif OS_FreeBSD
# include <sys/types.h>
# include <sys/sysctl.h>
#endif

#ifdef HAVE_MACH_ABSOLUTE_TIME
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

namespace Rct {

bool readFile(const Path& path, String& data)
{
    FILE* f = fopen(path.nullTerminated(), "r");
    if (!f)
        return false;
    const int sz = fileSize(f);
    if (!sz) {
        data.clear();
        return true;
    }
    data.resize(sz);
    const int r = fread(data.data(), sz, 1, f);
    fclose(f);
    return (r == 1);
}

bool writeFile(const Path& path, const String& data)
{
    FILE* f = fopen(path.nullTerminated(), "w");
    if (!f) {
        // try to make the directory and reopen
        const Path parent = path.parentDir();
        if (parent.isEmpty())
            return false;
        Path::mkdir(parent, Path::Recursive);
        f = fopen(path.nullTerminated(), "w");
        if (!f)
            return false;
    }
    const int w = fwrite(data.data(), data.size(), 1, f);
    fclose(f);
    if (w != 1) {
        unlink(path.nullTerminated());
        return false;
    }
    return true;
}

int readLine(FILE *f, char *buf, int max)
{
    assert(!buf == (max == -1));
    if (max == -1)
        max = INT_MAX;
    for (int i=0; i<max; ++i) {
        const int ch = fgetc(f);
        switch (ch) {
        case EOF:
            if (!i)
                i = -1;
            // fall through
        case '\n':
            if (buf)
                *buf = '\0';
            return i;
        }
        if (buf)
            *buf++ = *reinterpret_cast<const char*>(&ch);
    }
    return -1;
}


String shortOptions(const option *longOptions)
{
    String ret;
    for (int i=0; longOptions[i].name; ++i) {
        if (longOptions[i].val) {
            if (ret.contains(longOptions[i].val)) {
                printf("%c (%s) is already used\n", longOptions[i].val, longOptions[i].name);
                assert(!ret.contains(longOptions[i].val));
            }
            ret.append(longOptions[i].val);
            switch (longOptions[i].has_arg) {
            case no_argument:
                break;
            case optional_argument:
                ret.append("::");
                break;
            case required_argument:
                ret.append(':');
                break;
            default:
                assert(0);
                break;
            }
        }
    }
#if 0
    String unused;
    for (char ch='a'; ch<='z'; ++ch) {
        if (!ret.contains(ch)) {
            unused.append(ch);
        }
        const char upper = toupper(ch);
        if (!ret.contains(upper)) {
            unused.append(upper);
        }
    }
    printf("Unused letters: %s\n", unused.nullTerminated());
#endif
    return ret;
}

void removeDirectory(const Path &path)
{
    DIR *d = opendir(path.constData());
    size_t path_len = path.size();
    char buf[PATH_MAX];
    dirent *dbuf = reinterpret_cast<dirent*>(buf);

    if (d) {
        dirent *p;

        while (!readdir_r(d, dbuf, &p) && p) {
            char *buf;
            size_t len;

            /* Skip the names "." and ".." as we don't want to recurse on them. */
            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
                continue;
            }

            len = path_len + strlen(p->d_name) + 2;
            buf = static_cast<char*>(malloc(len));

            if (buf) {
                struct stat statbuf;
                snprintf(buf, len, "%s/%s", path.constData(), p->d_name);
                if (!stat(buf, &statbuf)) {
                    if (S_ISDIR(statbuf.st_mode)) {
                        removeDirectory(buf);
                    } else {
                        unlink(buf);
                    }
                }

                free(buf);
            }
        }
        closedir(d);
    }
    rmdir(path.constData());
}

static Path sExecutablePath;
Path executablePath()
{
    return sExecutablePath;
}

void findExecutablePath(const char *argv0)
{
    {
        assert(argv0);
        Path p = argv0;
        if (p.isFile()) {
            if (p.isAbsolute()) {
                sExecutablePath = p;
                return;
            }
            if (p.startsWith("./"))
                p.remove(0, 2);
            p.prepend(Path::pwd());
            if (p.isFile()) {
                sExecutablePath = p;
                return;
            }
        }
    }
    const char *path = getenv("PATH");
    const List<String> paths = String(path).split(':');
    for (int i=0; i<paths.size(); ++i) {
        const Path p = (paths.at(i) + "/") + argv0;
        if (p.isFile()) {
            sExecutablePath = p;
            return;
        }
    }
#if defined(OS_Linux)
    char buf[32];
    const int w = snprintf(buf, sizeof(buf), "/proc/%d/exe", getpid());
    Path p(buf, w);
    if (p.isSymLink()) {
        sExecutablePath = p.followLink();
        if (sExecutablePath.isFile())
            return;
    }
#elif defined(OS_Darwin)
    {
        char path[PATH_MAX];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            sExecutablePath = Path(path, size).followLink();
            if (sExecutablePath.isFile())
                return;
        }
    }
#elif defined(OS_FreeBSD)
    {
        int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
        char path[PATH_MAX];
        size_t size = sizeof(path);
        if (!sysctl(mib, 4, path, &size, 0, 0)) {
            sExecutablePath = Path(path, size).followLink();
            if (sExecutablePath.isFile())
                return;
        }
    }
#else
#warning Unknown platform.
#endif
    fprintf(stderr, "Can't find applicationDirPath");
}

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#include <cxxabi.h>

static inline char *demangle(const char *str)
{
    if (!str)
        return 0;
    int status;
#ifdef OS_Darwin
    char paren[1024];
    sscanf(str, "%*d %*s %*s %s %*s %*d", paren);
#else
    const char *paren = strchr(str, '(');
    if (!paren) {
        paren = str;
    } else {
        ++paren;
    }
#endif
    size_t l;
    if (const char *plus = strchr(paren, '+')) {
        l = plus - paren;
    } else {
        l = strlen(paren);
    }

    char buf[1024];
    size_t len = sizeof(buf);
    if (l >= len)
        return 0;
    memcpy(buf, paren, l + 1);
    buf[l] = '\0';
    char *ret = abi::__cxa_demangle(buf, 0, 0, &status);
    if (status != 0) {
        if (ret)
            free(ret);
#ifdef OS_Darwin
        return strdup(paren);
#else
        return 0;
#endif
    }
    return ret;
}

String backtrace(int maxFrames)
{
    enum { SIZE = 1024 };
    void *stack[SIZE];

    int frameCount = backtrace(stack, sizeof(stack) / sizeof(void*));
    if (frameCount <= 0)
        return String("Couldn't get stack trace");
    String ret;
    char **symbols = backtrace_symbols(stack, frameCount);
    if (symbols) {
        char frame[1024];
        for (int i=1; i<frameCount && (maxFrames < 0 || i - 1 < maxFrames); ++i) {
            char *demangled = demangle(symbols[i]);
            snprintf(frame, sizeof(frame), "%d/%d %s\n", i, frameCount - 1, demangled ? demangled : symbols[i]);
            ret += frame;
            if (demangled)
                free(demangled);
        }
        free(symbols);
    }
    return ret;
}
#else
String backtrace(int)
{
    return String();
}
#endif


bool gettime(timeval* time)
{
#if defined(HAVE_MACH_ABSOLUTE_TIME)
    static mach_timebase_info_data_t info;
    static bool first = true;
    uint64_t machtime = mach_absolute_time();
    if (first) {
        first = false;
        mach_timebase_info(&info);
    }
    machtime = machtime * info.numer / (info.denom * 1000); // microseconds
    time->tv_sec = machtime / 1000000;
    time->tv_usec = machtime % 1000000;
#elif defined(HAVE_CLOCK_MONOTONIC_RAW) || defined(HAVE_CLOCK_MONOTONIC)
    timespec spec;
#if defined(HAVE_CLOCK_MONOTONIC_RAW)
    const clockid_t cid = CLOCK_MONOTONIC_RAW;
#else
    const clockid_t cid = CLOCK_MONOTONIC;
#endif
    const int ret = ::clock_gettime(cid, &spec);
    if (ret == -1) {
        memset(time, 0, sizeof(timeval));
        return false;
    }
    time->tv_sec = spec.tv_sec;
    time->tv_usec = spec.tv_nsec / 1000;
#else
#error No EventLoop::gettime() implementation
#endif
    return true;
}

uint64_t monoMs()
{
    timeval time;
    if (gettime(&time)) {
        return (time.tv_sec * static_cast<uint64_t>(1000)) + (time.tv_usec / static_cast<uint64_t>(1000));
    }
    return 0;
}

uint64_t currentTimeMs()
{
    timeval time;
    gettimeofday(&time, NULL);
    return (time.tv_sec * static_cast<uint64_t>(1000)) + (time.tv_usec / static_cast<uint64_t>(1000));
}
} // namespace Rct

#ifdef RCT_DEBUG_MUTEX
void Mutex::lock()
{
    Timer timer;
    while (!tryLock()) {
        usleep(10000);
        if (timer.elapsed() >= 10000) {
            error("Couldn't acquire lock in 10 seconds\n%s", Rct::backtrace().constData());
            timer.restart();
        }
    }
}
#endif
