#include "Rct.h"

#include <limits.h>
#include <sys/time.h>
#include <unistd.h>
#include <getopt.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>
#ifdef OS_Darwin
# include <mach-o/dyld.h>
#elif defined(OS_FreeBSD) || defined(OS_DragonFly)
# include <netinet/in.h>
# include <sys/sysctl.h>
#endif
#ifdef _WIN32
#  ifdef _WIN32_WINNT
#    if _WIN32_WINNT < _WIN32_WINNT_VISTA
#      warning "need to compile at least for windows vista"
#    endif
#  else
#    define _WIN32_WINNT _WIN32_WINNT_VISTA
#    define NTDDI_VERSION NTDDI_VISTA
#  endif
#  include <Winsock2.h>
#  include <Ws2tcpip.h>
#  ifndef HOST_NAME_MAX
#    define HOST_NAME_MAX 256 //according to gethostname documentation on MSDN
#  endif
#else
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <sys/socket.h>
#endif

#include "rct/rct-config.h"
#include "rct/Path.h"
#ifdef HAVE_MACH_ABSOLUTE_TIME
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

#include "Log.h"

struct timeval;

#if !defined(HOST_NAME_MAX) && defined(_POSIX_HOST_NAME_MAX)
#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#endif

namespace Rct {

bool readFile(const Path& path, String& data, mode_t *perm)
{
    if (!path.isFile())
        return false;
    FILE *f = fopen(path.nullTerminated(), "r");
    if (!f)
        return false;
    const bool ret = readFile(f, data, perm);
    fclose(f);
    return ret;
}

bool readFile(FILE *f, String& data, mode_t *perm)
{
    assert(f);
    const int sz = fileSize(f);
    if (!sz) {
        data.clear();
        return true;
    }
    data.resize(sz);
    if (!fread(data.data(), sz, 1, f))
        return false;
    if (perm) {
        struct stat st;
        if (fstat(fileno(f), &st))
            return false;
        *perm = st.st_mode;
    }

    return true;
}

bool writeFile(const Path& path, const String& data, int perm)
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
    if (perm >= 0)
        chmod(path.constData(), static_cast<mode_t>(perm));

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

String readAll(FILE *f, int max)
{
    assert(f);
    int fd = fileno(f);
    assert(fd != -1);
    struct stat st;
    if (!fstat(fd, &st)) {
        int size = static_cast<int>(st.st_size);
        if (max > 0 && max < size)
            size = max;
        String buf(size, '\0');
        if (size) {
            const int ret = fread(buf.data(), sizeof(char), size, f);
            if (ret == size)
                return buf;
        }
    }
    return String();
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

    const List<String> paths = String(path).split(Path::ENV_PATH_SEPARATOR);
    for (size_t i=0; i<paths.size(); ++i) {
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
        char buf[PATH_MAX];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0) {
            sExecutablePath = Path(buf, size).followLink();
            if (sExecutablePath.isFile())
                return;
        }
    }
#elif defined(OS_FreeBSD) || defined(OS_DragonFly)
    {
        int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
        char buf[PATH_MAX];
        size_t size = sizeof(buf);
        if (!sysctl(mib, 4, buf, &size, 0, 0)) {
            sExecutablePath = Path(buf, size).followLink();
            if (sExecutablePath.isFile())
                return;
        }
    }
#elif defined _WIN32
    //nothing here so far.
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
        return nullptr;
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
        return nullptr;
    memcpy(buf, paren, l + 1);
    buf[l] = '\0';
    char *ret = abi::__cxa_demangle(buf, nullptr, nullptr, &status);
    if (status != 0) {
        if (ret)
            free(ret);
#ifdef OS_Darwin
        return strdup(paren);
#else
        return nullptr;
#endif
    }
    return ret;
}

String backtrace(int maxFrames)
{
    enum { SIZE = 1024 };
    void *stack[SIZE];

    const int frameCount = backtrace(stack, sizeof(stack) / sizeof(void*));
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
    gettimeofday(&time, nullptr);
    return (time.tv_sec * static_cast<uint64_t>(1000)) + (time.tv_usec / static_cast<uint64_t>(1000));
}

String currentTimeString()
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    enum {
        SEC_PER_MIN = 60,
        SEC_PER_HOUR = SEC_PER_MIN * 60,
        SEC_PER_DAY = SEC_PER_HOUR * 24
    };

    long hms = tv.tv_sec % SEC_PER_DAY;
    hms += tz.tz_dsttime * SEC_PER_HOUR;
    hms -= tz.tz_minuteswest * SEC_PER_MIN;
    hms = (hms + SEC_PER_DAY) % SEC_PER_DAY;

    int hour = hms / SEC_PER_HOUR;
    int min = (hms % SEC_PER_HOUR) / SEC_PER_MIN;
    int sec = (hms % SEC_PER_HOUR) % SEC_PER_MIN; // or hms % SEC_PER_MIN

    return String::format<16>("%d:%02d:%02d.%03llu",
                              hour, min, sec, tv.tv_usec / static_cast<unsigned long long>(1000));
}

String hostName()
{
    String host(HOST_NAME_MAX, '\0');
    ::gethostname(host.data(), HOST_NAME_MAX);
    host.resize(strlen(host.constData()));
    return host;
}

const char *colors[] = {
    "\x1b[0m", // Default
    "\x1b[30m", // Black
    "\x1b[31m", // Red
    "\x1b[32m", // Green
    "\x1b[33m", // Yellow
    "\x1b[34m", // Blue
    "\x1b[35m", // Magenta
    "\x1b[36m", // Cyan
    "\x1b[37m", // White
    "\x1b[0;1m", // BrightDefault
    "\x1b[30;1m", // BrightBlack
    "\x1b[31;1m", // BrightRed
    "\x1b[32;1m", // BrightGreen
    "\x1b[33;1m", // BrightYellow
    "\x1b[34;1m", // BrightBlue
    "\x1b[35;1m", // BrightMagenta
    "\x1b[36;1m", // BrightCyan
    "\x1b[37;1m" // BrightWhite
};

String colorize(const String &string, AnsiColor color, size_t from, size_t len)
{
    assert(from <= string.size());
    if (len == String::npos) {
        len = string.size() - from;
    }
    assert(from + len <= string.size());
    if (!len)
        return string;

    String ret;
    ret.reserve(string.size() + 20);
    const char *str = string.constData();
    if (from > 0) {
        ret.append(str, from);
        str += from;
    }
    ret.append(colors[color]);
    ret.append(str, len);
    str += len;
    ret.append(colors[AnsiColor_Default]);
    if (from + len != string.size())
        ret.append(str, string.size() - from - len);

    return ret;
}

bool isIP(const String& addr, LookupMode mode)
{
    union {
        sockaddr_storage sockaddr;
        sockaddr_in6 sockaddr6;
        sockaddr_in sockaddr4;
    };
    memset(&sockaddr, 0, sizeof(sockaddr_storage));
    if (mode == Auto)
        mode = addr.contains(':') ? IPv6 : IPv4;
    if (mode == IPv6) {
        if (inet_pton(AF_INET6, addr.constData(), &sockaddr6.sin6_addr) != 1)
            return false;
    } else {
        if (inet_pton(AF_INET, addr.constData(), &sockaddr4.sin_addr) != 1)
            return false;
    }
    return true;
}

String addrLookup(const String &address, LookupMode mode, bool *ok)
{
    union {
        sockaddr_storage sockaddrStorage;
        sockaddr_in6 sockaddr6;
        sockaddr_in sockaddr4;
        sockaddr addr;
    };
    memset(&sockaddrStorage, 0, sizeof(sockaddr_storage));
    size_t sz;
    if (mode == Auto)
        mode = address.contains(':') ? IPv6 : IPv4;
    if (mode == IPv6) {
        if (inet_pton(AF_INET6, address.constData(), &sockaddr6.sin6_addr) != 1) {
            if (ok)
                *ok = false;
            return address;
        }
        sockaddrStorage.ss_family = AF_INET6;
        sz = sizeof(sockaddr_in6);
    } else {
        if (inet_pton(AF_INET, address.constData(), &sockaddr4.sin_addr) != 1) {
            if (ok)
                *ok = false;
            return address;
        }
        sockaddrStorage.ss_family = AF_INET;
        sz = sizeof(sockaddr_in);
    }
    String out(NI_MAXHOST, '\0');
    if (getnameinfo(&addr, sz, out.data(), NI_MAXHOST, nullptr, 0, 0) != 0) {
        if (ok)
            *ok = false;
        // bad
        return address;
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

    if (getaddrinfo(name.constData(), nullptr, &hints, &res) != 0) {
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

String strerror(int error)
{
#ifdef _GNU_SOURCE
#undef _GNU_SOURCE
#endif
#ifdef __USE_GNU
#undef __USE_GNU
#endif
    char buf[1024];
    buf[0] = '\0';
#ifdef _WIN32
    auto foo = strerror_s(buf, sizeof(buf), error);
#else
    auto foo = strerror_r(error, buf, sizeof(buf));
#endif
    (void)foo;
    String ret = buf;
    ret << " (" << error << ')';
    return ret;
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
