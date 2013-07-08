#include "rct/Path.h"
#include "rct/Log.h"
#include "rct/Rct.h"
#include "rct-config.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

// this doesn't check if *this actually is a real file
Path Path::parentDir() const
{
    if (isEmpty())
        return Path();
    if (size() == 1 && at(0) == '/')
        return Path();
    Path copy = *this;
    int i = copy.size() - 1;
    while (copy.at(i) == '/')
        --i;
    while (i >= 0 && copy.at(i) != '/')
        --i;
    if (i < 0)
        return Path();
    copy.truncate(i + 1);
    assert(copy.endsWith('/'));
    return copy;
}

Path::Type Path::type() const
{
    struct stat st;
    if (stat(constData(), &st) == -1)
        return Invalid;

    switch (st.st_mode & S_IFMT) {
    case S_IFBLK: return BlockDevice;
    case S_IFCHR: return CharacterDevice;
    case S_IFDIR: return Directory;
    case S_IFIFO: return NamedPipe;
    case S_IFREG: return File;
    case S_IFSOCK: return Socket;
    default:
        break;
    }
    return Invalid;
}

bool Path::isSymLink() const
{
    struct stat st;
    if (lstat(constData(), &st) == -1)
        return false;

    return (st.st_mode & S_IFMT) == S_IFLNK;
}

mode_t Path::mode() const
{
    struct stat st;
    if (stat(constData(), &st) == -1)
        return 0;

    return st.st_mode;
}


time_t Path::lastModified() const
{
    struct stat st;
    if (stat(constData(), &st) == -1) {
        warning("Stat failed for %s", constData());
        return 0;
    }
    return st.st_mtime;
}

int64_t Path::fileSize() const
{
    struct stat st;
    if (!stat(constData(), &st)) {// && st.st_mode == S_IFREG)
        return st.st_size;
    }
    return -1;
}

Path Path::resolved(const String &path, ResolveMode mode, const Path &cwd, bool *ok)
{
    Path ret(path);
    if (ret.resolve(mode, cwd) && ok) {
        *ok = true;
    } else if (ok) {
        *ok = false;
    }
    return ret;
}

int Path::canonicalize()
{
    int len = size();
    char *path = data();
    for (int i=0; i<len - 1; ++i) {
        if (path[i] == '/') {
            if (i + 3 < len && path[i + 1] == '.' && path[i + 2] == '.' && path[i + 3] == '/') {
                for (int j=i - 1; j>=0; --j) {
                    if (path[j] == '/') {
                        memmove(path + j, path + i + 3, len - (i + 2));
                        const int removed = (i + 3 - j);
                        len -= removed;
                        i -= removed;
                        break;
                    }
                }
            } else if (path[i + 1] == '/') {
                memmove(path + i, path + i + 1, len - (i + 1));
                --i;
                --len;
            }
        }
    }

    if (len != size())
        truncate(len);
    return len;
}

Path Path::canonicalized() const
{
    Path ret = *this;
    const int c = ret.canonicalize();
    if (c != size())
        return ret;
    return *this; // better chance of being implicity shared :-)
}

Path Path::canonicalized(const Path &path)
{
    Path p = path;
    if (p.canonicalize() != path.size())
        return p;
    return path; // same as above
}

Path Path::resolved(ResolveMode mode, const Path &cwd, bool *ok) const
{
    Path ret = *this;
    if (ret.resolve(mode, cwd) && ok) {
        *ok = true;
    } else if (ok) {
        *ok = false;
    }
    return ret;
}

bool Path::resolve(ResolveMode mode, const Path &cwd)
{
    if (mode == MakeAbsolute) {
        if (isAbsolute())
            return true;
        const Path copy = (cwd.isEmpty() ? Path::pwd() : cwd) + *this;
        if (copy.exists()) {
            operator=(copy);
            return true;
        }
        return false;
    } else {
        if (!cwd.isEmpty() && !isAbsolute()) {
            Path copy = cwd + '/' + *this;
            if (copy.resolve(RealPath)) {
                operator=(copy);
                return true;
            }
        }

        {
            char buffer[PATH_MAX + 2];
            if (realpath(constData(), buffer)) {
                String::operator=(buffer);
                return true;
            }
        }
    }
    return false;
}

const char * Path::fileName(int *len) const
{
    const int idx = lastIndexOf('/') + 1;
    if (len)
        *len = size() - idx;
    return constData() + idx;
}

const char * Path::extension() const
{
    const int dot = lastIndexOf('.');
    if (dot == -1 || dot + 1 == size())
        return 0;
    return constData() + dot + 1;
}

bool Path::isSource(const char *ext)
{
    const char *sources[] = { "c", "cc", "cpp", "cxx", "moc", 0 };
    for (int i=0; sources[i]; ++i) {
        if (!strcasecmp(ext, sources[i]))
            return true;
    }
    return false;
}

bool Path::isSource() const
{
    if (exists()) {
        const char *ext = extension();
        if (ext)
            return isSource(ext);
    }
    return false;
}

bool Path::isHeader() const
{
    if (exists()) {
        const char *ext = extension();
        if (ext)
            return isHeader(ext);
    }
    return false;
}

bool Path::isHeader(const char *ext)
{
    const char *headers[] = { "h", "hpp", "hxx", "hh", "tcc", 0 };
    for (int i=0; headers[i]; ++i) {
        if (!strcasecmp(ext, headers[i]))
            return true;
    }
    return false;
}

bool Path::isSystem(const char *path)
{
    if (!strncmp("/usr/", path, 5)) {
#ifdef OS_FreeBSD
        if (!strncmp("home/", path + 5, 5))
            return false;
#endif
        return true;
    }
#ifdef OS_Darwin
    if (!strncmp("/System/", path, 8))
        return true;
#endif
    return false;
}

Path Path::canonicalized(const String &path)
{
    Path p(path);
    p.canonicalize();
    return p;
}

bool Path::mksubdir(const String &path) const
{
    if (isDir()) {
        String combined = *this;
        if (!combined.endsWith('/'))
            combined.append('/');
        combined.append(path);
        return Path::mkdir(combined);
    }
    return false;
}

bool Path::mkdir(const Path &path, MkDirMode mkdirMode, mode_t permissions)
{
    errno = 0;
    if (!::mkdir(path.constData(), permissions) || errno == EEXIST || errno == EISDIR)
        return true;
    if (mkdirMode == Single)
        return false;
    if (path.size() > PATH_MAX)
        return false;

    char buf[PATH_MAX + 2];
    strcpy(buf, path.constData());
    int len = path.size();
    if (!path.endsWith('/')) {
        buf[len++] = '/';
        buf[len] = '\0';
    }

    for (int i = 1; i < len; ++i) {
        if (buf[i] == '/') {
            buf[i] = 0;
            const int r = ::mkdir(buf, permissions);
            if (r && errno != EEXIST && errno != EISDIR)
                return false;
            buf[i] = '/';
        }
    }
    return true;
}

bool Path::rm(const Path &file)
{
    return !unlink(file.constData());
}

void Path::visit(VisitCallback callback, void *userData) const
{
    if (!callback)
        return;
    DIR *d = opendir(constData());
    if (!d)
        return;
    char buf[PATH_MAX + sizeof(dirent) + 1];
    dirent *dbuf = reinterpret_cast<dirent*>(buf);

    dirent *p;
    Path path = *this;
    if (!path.endsWith('/'))
        path.append('/');
    const int s = path.size();
    path.reserve(s + 128);
    List<String> recurseDirs;
    while (!readdir_r(d, dbuf, &p) && p) {
        if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
            continue;
        path.truncate(s);
        path.append(p->d_name);
#if defined(_DIRENT_HAVE_D_TYPE) && defined(_BSD_SOURCE)
        if (p->d_type == DT_DIR)
            path.append('/');
#else
        if (path.isDir())
            path.append('/');
#endif
        switch (callback(path, userData)) {
        case Abort:
            p = 0;
            break;
        case Recurse:
            if (path.isDir())
                recurseDirs.append(p->d_name);
            break;
        case Continue:
            break;
        }
    }
    closedir(d);
    const int count = recurseDirs.size();
    for (int i=0; i<count; ++i) {
        path.truncate(s);
        path.append(recurseDirs.at(i));
        path.visit(callback, userData);
    }
}

Path Path::followLink(bool *ok) const
{
    if (isSymLink()) {
        char buf[PATH_MAX];
        int w = readlink(constData(), buf, sizeof(buf) - 1);
        if (w != -1) {
            if (ok)
                *ok = true;
            buf[w] = '\0';
            return buf;
        }
    }
    if (ok)
        *ok = false;

    return *this;
}

int Path::readAll(char *&buf, int max) const
{
    FILE *f = fopen(constData(), "r");
    buf = 0;
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    int size = ftell(f);
    if (max > 0 && max < size)
        size = max;
    if (size) {
        fseek(f, 0, SEEK_SET);
        buf = new char[size + 1];
        const int ret = fread(buf, sizeof(char), size, f);
        if (ret != size) {
            size = -1;
            delete[] buf;
        } else {
            buf[size] = '\0';
        }
    }
    fclose(f);
    return size;
}

String Path::readAll(int max) const
{
    FILE *f = fopen(constData(), "r");
    if (!f)
        return String();
    fseek(f, 0, SEEK_END);
    int size = ftell(f);
    if (max > 0 && max < size)
        size = max;
    String buf(size, '\0');
    if (size) {
        fseek(f, 0, SEEK_SET);
        const int ret = fread(buf.data(), sizeof(char), size, f);
        if (ret != size)
            buf.clear();
    }
    fclose(f);
    return buf;
}

Path Path::home()
{
    Path ret = Path::resolved(getenv("HOME"));
    if (!ret.endsWith('/'))
        ret.append('/');
    return ret;
}

Path Path::toTilde() const
{
    const Path home = Path::home();
    if (startsWith(home))
        return String::format<64>("~/%s", constData() + home.size());
    return *this;
}

Path Path::pwd()
{
    char buf[PATH_MAX];
    char *pwd = getcwd(buf, sizeof(buf));
    if (pwd) {
        Path ret(pwd);
        if (!ret.endsWith('/'))
            ret.append('/');
        return ret;
    }
    return Path();
}
struct FilesUserData
{
    unsigned filter;
    int max;
    bool recurse;
    List<Path> paths;
};
static Path::VisitResult filesVisitor(const Path &path, void *userData)
{
    FilesUserData &u = *reinterpret_cast<FilesUserData*>(userData);
    if (u.max > 0)
        --u.max;
    if (path.type() & u.filter) {
        u.paths.append(path);
    }
    if (!u.max)
        return Path::Abort;
    return u.recurse ? Path::Recurse : Path::Continue;
}

List<Path> Path::files(unsigned filter, int max, bool recurse) const
{
    assert(max != 0);
    FilesUserData userData = { filter, max, recurse, List<Path>() };
    visit(::filesVisitor, &userData);
    return userData.paths;
}

uint64_t Path::lastModifiedMs() const
{
    struct stat st;
    if (stat(constData(), &st) == -1) {
        warning("Stat failed for %s", constData());
        return 0;
    }
#ifdef HAVE_STATMTIM
    return st.st_mtim.tv_sec * static_cast<uint64_t>(1000) + st.st_mtim.tv_nsec / static_cast<uint64_t>(1000000);
#else
    return st.st_mtime * static_cast<uint64_t>(1000);
#endif
}
