#include "rct/Rct.h"
#include "rct/Log.h"
#include <sys/types.h>
#include <dirent.h>
#include <sys/fcntl.h>
#ifdef OS_Darwin
# include <mach-o/dyld.h>
#elif OS_FreeBSD
# include <sys/types.h>
# include <sys/sysctl.h>
#endif

namespace Rct {

int canonicalizePath(char *path, int len)
{
    assert(path[0] == '/');
    for (int i=0; i<len - 3; ++i) {
        if (path[i] == '/' && path[i + 1] == '.'
            && path[i + 2] == '.' && path[i + 3] == '/') {
            for (int j=i - 1; j>=0; --j) {
                if (path[j] == '/') {
                    memmove(path + j, path + i + 3, len - (i + 2));
                    const int removed = (i + 3 - j);
                    len -= removed;
                    i -= removed;
                    break;
                }
            }
        }
    }
    return len;
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
        if (ret.contains(longOptions[i].val)) {
            error("%c (%s) is already used", longOptions[i].val, longOptions[i].name);
            assert(!ret.contains(longOptions[i].val));
        }
        ret.append(longOptions[i].val);
        switch (longOptions[i].has_arg) {
        case no_argument:
            break;
        case optional_argument:
            ret.append(':');
            ret.append(':');
            break;
        case required_argument:
            ret.append(':');
            break;
        default:
            assert(0);
            break;
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
bool startProcess(const Path &dotexe, const List<String> &dollarArgs)
{
    switch (fork()) {
    case 0:
        break;
    case -1:
        return false;
    default:
        return true;
    }

    if (setsid() < 0)
        _exit(1);


    switch (fork()) {
    case 0:
        break;
    case -1:
        _exit(1);
    default:
        _exit(0);
    }

    int ret = chdir("/");
    if (ret == -1)
        perror("Rct::startProcess() Failed to chdir(\"/\")");

    umask(0);

    const int fdlimit = sysconf(_SC_OPEN_MAX);
    for (int i=0; i<fdlimit; ++i)
        close(i);

    open("/dev/null", O_RDWR);
    ret = dup(0);
    if (ret == -1)
        perror("Rct::startProcess() Failed to duplicate fd");
    ret = dup(0);
    if (ret == -1)
        perror("Rct::startProcess() Failed to duplicate fd");
    char **args = new char*[dollarArgs.size() + 2];
    args[0] = strndup(dotexe.constData(), dotexe.size());
    for (int i=0; i<dollarArgs.size(); ++i) {
        args[i + 1] = strndup(dollarArgs.at(i).constData(), dollarArgs.at(i).size());
    }
    args[dollarArgs.size() + 1] = 0;
    execvp(dotexe.constData(), args);
    FILE *f = fopen("/tmp/failedtolaunch", "w");
    if (f) {
        fwrite(dotexe.constData(), 1, dotexe.size(), f);
        fwrite(" ", 1, 1, f);
        const String joined = String::join(dollarArgs, " ");
        fwrite(joined.constData(), 1, joined.size(), f);
        fclose(f);
    }
    _exit(1);
    return false;
}

static Path sExecutablePath;
Path executablePath()
{
    return sExecutablePath;
}

void findExecutablePath(const char *argv0)
{
#if defined(OS_Linux)
    char buf[32];
    const int w = snprintf(buf, sizeof(buf), "/proc/%d/exe", getpid());
    Path p(buf, w);
    if (p.isSymLink()) {
        sExecutablePath = Path(path, size).followLink();
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
    {
        assert(argv0);
        Path a(argv0);
        if (a.resolve(Path::MakeAbsolute) && a.isFile()) {
            sExecutablePath = a;
            return;
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

}

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

