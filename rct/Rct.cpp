#include "Rct.h"
#include "Log.h"
#include "rct-config.h"
#include <sys/time.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#ifdef OS_Darwin
# include <mach-o/dyld.h>
#elif OS_FreeBSD
# include <sys/sysctl.h>
# include <netinet/in.h>
#endif

#ifdef HAVE_MACH_ABSOLUTE_TIME
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

#if !defined(HOST_NAME_MAX) && defined(_POSIX_HOST_NAME_MAX)
#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#endif

namespace Rct {

bool readFile(const Path& path, String& data)
{
    FILE *f = fopen(path.nullTerminated(), "r");
    if (!f)
        return false;
    const bool ret = readFile(f, data);
    fclose(f);
    return ret;
}

bool readFile(FILE *f, String& data)
{
    assert(f);
    const int sz = fileSize(f);
    if (!sz) {
        data.clear();
        return true;
    }
    data.resize(sz);
    return fread(data.data(), sz, 1, f);
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
    char bufbuf[16384];
    if (!buf)
        max = sizeof(bufbuf);
    char *ret = fgets(buf ? buf : bufbuf, max, f);
    if (ret) {
        int len = strlen(ret);
        if (len && ret[len - 1] == '\n') {
            ret[--len] = '\0';
        }
        return len;
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
#if defined(OS_Linux) || defined(__CYGWIN__)
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

String hostName()
{
    String host(HOST_NAME_MAX, '\0');
    ::gethostname(host.data(), HOST_NAME_MAX);
    host.resize(strlen(host.constData()));
    return host;
}

String addrLookup(const String& addr, LookupMode mode, bool *ok)
{
    sockaddr_storage sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr_storage));
    size_t sz;
    if (mode == Auto)
        mode = addr.contains(':') ? IPv6 : IPv4;
    if (mode == IPv6) {
        sockaddr_in6* sockaddr6 = reinterpret_cast<sockaddr_in6*>(&sockaddr);
        if (inet_pton(AF_INET6, addr.constData(), &sockaddr6->sin6_addr) != 1) {
            if (ok)
                *ok = false;
            return addr;
        }
        sockaddr.ss_family = AF_INET6;
        sz = sizeof(sockaddr_in6);
    } else {
        sockaddr_in* sockaddr4 = reinterpret_cast<sockaddr_in*>(&sockaddr);
        if (inet_pton(AF_INET, addr.constData(), &sockaddr4->sin_addr) != 1) {
            if (ok)
                *ok = false;
            return addr;
        }
        sockaddr.ss_family = AF_INET;
        sz = sizeof(sockaddr_in);
    }
    String out(NI_MAXHOST, '\0');
    const struct sockaddr* sa = reinterpret_cast<struct sockaddr*>(&sockaddr);
    if (getnameinfo(sa, sz, out.data(), NI_MAXHOST, 0, 0, 0) != 0) {
        if (ok)
            *ok = false;
        // bad
        return addr;
    }
    out.resize(strlen(out.constData()));
    if (ok)
        *ok = true;

    return out;
}

String nameLookup(const String& name, LookupMode mode, bool *ok)
{
    assert(mode == Auto);
    String out(INET6_ADDRSTRLEN, '\0');
    addrinfo hints, *p, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = (mode == IPv6) ? AF_INET6 : AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(name.constData(), NULL, &hints, &res) != 0) {
        if (ok)
            *ok = false;
        // bad
        return name;
    }

    bool found = false;
    for (p = res; p; p = p->ai_next) {
        if (mode == IPv4 && p->ai_family == AF_INET) {
            sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(p->ai_addr);
            inet_ntop(AF_INET, &addr->sin_addr, out.data(), out.size());
            out.resize(strlen(out.constData()));
            found = true;
            break;
        } else if (mode == IPv6 && p->ai_family == AF_INET6) {
            sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(p->ai_addr);
            inet_ntop(AF_INET6, &addr->sin6_addr, out.data(), out.size());
            out.resize(strlen(out.constData()));
            found = true;
            break;
        }
    }

    freeaddrinfo(res);

    if (!found) {
        if (ok)
            *ok = false;
        return name;
    }
    if (ok)
        *ok = true;

    return out;
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
