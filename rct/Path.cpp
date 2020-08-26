#include "Path.h"

#include <dirent.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "StackBuffer.h"
#include "rct/List.h"
#include "rct/Path.h"
#include "rct/Set.h"
#include "rct/String.h"
#ifdef _WIN32
#  include <direct.h>
#  include <Windows.h>
#  include <Shellapi.h>

#  include "WindowsUnicodeConversion.h"
#else
#  include <wordexp.h>
#endif
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <functional>
#include <iosfwd>

#include "Rct.h"
#include "rct/rct-config.h"

bool Path::sRealPathEnabled = true;

#ifdef _WIN32
const char Path::ENV_PATH_SEPARATOR = ';';
#else
const char Path::ENV_PATH_SEPARATOR = ':';
#endif

// this doesn't check if *this actually is a real file
Path Path::parentDir() const
{
    if (isEmpty())
        return Path();
    if (size() == 1 && at(0) == '/')
        return Path();
    Path copy = *this;
    int i = copy.size() - 1;
    while (i >= 0 && copy.at(i) == '/')
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
    bool ok;
    struct stat st = stat(&ok);
    if (!ok)
        return Invalid;

    switch (st.st_mode & S_IFMT) {
    case S_IFBLK: return BlockDevice;
    case S_IFCHR: return CharacterDevice;
    case S_IFDIR: return Directory;
    case S_IFIFO: return NamedPipe;
    case S_IFREG: return File;
#ifndef _WIN32   // no socket files on windows
    case S_IFSOCK: return Socket;
#endif
    default:
        break;
    }
    return Invalid;
}

bool Path::isAbsolute() const
{
#ifdef _WIN32

    // https://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx#fully_qualified_vs._relative_paths
    // absolute paths start with either:
    // - a drive letter, such as c:/
    // - a single backslash (is replaced by single forward slash by this class)
    // - double backslashes

    if(size() >= 3)
    {
        const char &first  = (*this)[0];
        const char &second = (*this)[1];
        const char &third  = (*this)[2];

        // check for regular windows path that starts with a driver letter,
        // e.g. C:/windows

        if(((first >= 'a' && first <= 'z') ||
            (first >= 'A' && first <= 'Z')) &&
           second == ':' && third == '/')
        {
            return true;
        }
    }

    if(size() >= 2 && (*this)[0] == '\\' && (*this)[1] == '\\')
    {
        // network or unc path
        return true;
    }
#endif

    return (!isEmpty() && at(0) == '/');
}

bool Path::isSymLink() const
{
#ifdef _WIN32
    return false;   // no symlinks on windows
#else
    bool ok;
    struct stat st = stat(&ok);
    if (!ok)
        return Invalid;

    return (st.st_mode & S_IFMT) == S_IFLNK;
#endif
}

mode_t Path::mode() const
{
    bool ok;
    struct stat st = stat(&ok);
    if (!ok)
        return 0;

    return st.st_mode;
}


time_t Path::lastModified() const
{
    bool ok;
    struct stat st = stat(&ok);
    if (!ok)
        return 0;
    return st.st_mtime;
}

time_t Path::lastAccess() const
{
    bool ok;
    struct stat st = stat(&ok);
    if (!ok)
        return 0;
    return st.st_atime;
}

bool Path::setLastModified(time_t lastModified) const
{
    struct utimbuf buf = { lastAccess(), lastModified };
    return !utime(constData(), &buf);
}

int64_t Path::fileSize() const
{
    bool ok;
    struct stat st = stat(&ok);
    if (!ok)
        return -1;

    return st.st_size;
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

size_t Path::canonicalize(bool *changed)
{
    size_t len = size();
    char *path = data();
    bool dummy;
    bool &ref = changed ? *changed : dummy;
    ref = false;
    for (size_t i=0; i<len - 1; ++i) {
        if (path[i] == '/') {
          if ((i + 3 < len && path[i + 1] == '.' && path[i + 2] == '.' && path[i + 3] == '/') ||
              (i + 3 == len && path[i + 1] == '.' && path[i + 2] == '.')) {
                for (int j=i - 1; j>=0; --j) {
                    if (path[j] == '/') {
                        memmove(path + j, path + i + 3, len - (i + 2));
                        const int removed = (i + 3 - j);
                        len -= removed;
                        i -= removed;
                        ref = true;
                        break;
                    }
                }
          } else if ((i + 2 < len && path[i + 1] == '.' && path[i + 2] == '/') ||
                     (i + 2 == len && path[i + 1] == '.')) {
                // skip over /./
                memmove(path + i, path + i + 2, len - (i + 1));
                ref = true;
                len -= 2;
                i -= 2;
            } else if (path[i + 1] == '/') {
                memmove(path + i, path + i + 1, len - i);
                ref = true;
                --i;
                --len;
            }
        }
    }
    // Remove trailing slash
    if (path[len - 1] == '/') {
        path[len - 1] = '\0';
        ref = true;
        --len;
    }

    if (len != size())
        truncate(len);
    return len;
}

Path Path::canonicalized() const
{
    Path ret = *this;
    const size_t c = ret.canonicalize();
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

bool Path::resolve(ResolveMode mode, const Path &cwd, bool *changed)
{
    if (changed)
        *changed = false;
#ifndef _WIN32
    if (isEmpty())
        return false;
    if (startsWith('~')) {
        wordexp_t exp_result;
        wordexp(constData(), &exp_result, 0);
        operator=(exp_result.we_wordv[0]);
        wordfree(&exp_result);
        if (changed)
            *changed = true;
    }
#endif
    if (*this == ".") {
        if (!cwd.isEmpty()) {
            operator=(cwd);
        } else {
            operator=(Path::pwd());
        }
        return true;
    }
    if (mode == MakeAbsolute || !sRealPathEnabled) {
        if (isAbsolute()) {
            canonicalize(changed);
            return true;
        }

        // we only add the relative file name to the cwd/pwd and check if the
        // result exists.
        Path copy = (cwd.isEmpty() ? Path::pwd() : cwd.ensureTrailingSlash()) + *this;
        if (copy.exists()) {
            if (changed)
                *changed = true;
            if (mode == RealPath)
                copy.canonicalize();
            operator=(copy);
            return true;
        }
        return false;
    }

    // we only get here if mode == RealPath

    if (!cwd.isEmpty() && !isAbsolute()) {
        //resolve relative path as a path relative to cwd
        Path copy = cwd + '/' + *this;
        if (copy.resolve(RealPath, Path(), changed)) {
            operator=(copy);
            return true;
        }
    }

    {
        char buffer[PATH_MAX + 2];
#ifdef _WIN32
        if (_fullpath(buffer, constData(), PATH_MAX)) {
#else
        if (realpath(constData(), buffer)) {
#endif
            if (isDir()) {
                // dirs usually don't have a trailing '/', so we add one
                const int len = strlen(buffer);
                assert(buffer[len] != '/');
                buffer[len] = '/';
                buffer[len + 1] = '\0';
            }
            if (changed && strcmp(buffer, constData()))
                *changed = true;
            *this = buffer;
            return true;
        }
    }

    return false;
}

const char *Path::fileName(size_t *len) const
{
    const int length = size();
    size_t idx = 0;
    if (length > 1)
        idx = lastIndexOf('/', length - 2) + 1;

    if (len)
        *len = size() - idx;
    return constData() + idx;
}

const char *Path::extension(size_t *len) const
{
    if (len)
        *len = 0;
    const size_t s = size();
    if (s) {
        int dot = s - 1;
        const char *data = constData();
        while (dot >= 0) {
            switch (data[dot]) {
            case '.':
                if (len)
                    *len = s - (dot + 1);
                return data + dot + 1;
            case '/':
                return nullptr;
            default:
                break;
            }
            --dot;
        }
    }
    return nullptr;
}

bool Path::isSource(const char *ext)
{
    const char *sources[] = { "c", "cc", "cpp", "cxx", "c++", "moc", "mm", "m", "cu", nullptr };
    for (size_t i=0; sources[i]; ++i) {
        if (!strcasecmp(ext, sources[i]))
            return true;
    }
    return false;
}

bool Path::isSource() const
{
    if (isFile()) {
        const char *ext = extension();
        if (ext)
            return isSource(ext);
    }
    return false;
}

bool Path::isHeader() const
{
    return isFile() && isHeader(extension());
}

bool Path::isHeader(const char *ext)
{
    if (!ext)
        return true;
    const char *headers[] = { "h", "hpp", "hxx", "hh", "tcc", "tpp", "txx", "inc", "cuh", nullptr };
    for (size_t i=0; headers[i]; ++i) {
        if (!strcasecmp(ext, headers[i]))
            return true;
    }
    return false;
}

bool Path::isSystem(const char *path)
{
    if (!strncmp("/usr/", path, 5)) {
#if defined(OS_FreeBSD) || defined(OS_DragonFly)
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
#ifdef _WIN32
    (void) permissions;   // unused on windows
    if (!::_wmkdir(Utf8To16(path.c_str())) || errno == EEXIST)
#else
    if (!::mkdir(path.constData(), permissions) || errno == EEXIST || errno == EISDIR)
#endif
    {
        // mkdir call succeeded or it failed because the dir already exists
        return true;
    }
    if (mkdirMode == Single)
        return false;
    if (path.size() > PATH_MAX)
        return false;

    // directory creation failed so far
    // because mkdirMode == Recursive, we go along the path and create
    // directories, continuing while dir creation is successful or it failed
    // only because directories already exist.

    char buf[PATH_MAX + 2];
    strcpy(buf, path.constData());
    size_t len = path.size();
    if (!path.endsWith('/')) {
        buf[len++] = '/';
        buf[len] = '\0';
    }

    for (size_t i = 1; i < len; ++i) {
        if (buf[i] == '/') {
            buf[i] = 0;
#ifdef _WIN32
            Utf8To16 tmp2(buf);
            const int r = ::_wmkdir(tmp2);
#else
            const int r = ::mkdir(buf, permissions);
#endif
            if (r && errno != EEXIST && errno != EISDIR
#ifdef OS_CYGWIN
                    // on cygwin/msys2 we may try to create something like (/cygdrive)/c/some/path/
                    // an mkdir() attempt to create /c/ will fail with EACCESS so we need to catch it here
                    && errno != EACCES
#endif
                    )
                return false;
            buf[i] = '/';
        }
    }
    return true;
}

bool Path::mkdir(MkDirMode mkdirMode, mode_t permissions) const
{
    return Path::mkdir(*this, mkdirMode, permissions);
}

bool Path::rm(const Path &file)
{
    return !unlink(file.constData());
}

bool Path::rmdir(const Path &dir)
{
#ifdef _WIN32
    // SHFileOperation needs an absolute path.
    Path absDir = Path::resolved(dir, MakeAbsolute);
    std::wstring pathU16 = Utf8To16(dir.c_str());

    // double zero terminate the string. There *should* already be one zero
    // in c_str(), but better to be safe.
    pathU16.append(2, 0);

    // Deletion through SHFileOperation is recursive.
    SHFILEOPSTRUCTW op;
    memset(&op, 0, sizeof(SHFILEOPSTRUCTW));
    op.wFunc = FO_DELETE;
    op.pFrom = pathU16.c_str();
    op.fFlags = FOF_NO_UI;
    bool ret = (SHFileOperationW(&op) == 0);
    return ret;

#else
    DIR *d = opendir(dir.constData());
    size_t path_len = dir.size();
    union {
        char buf[PATH_MAX];
        dirent dbuf;
    };

    if (d) {
        while (dirent *p = readdir(d)) {
            /* Skip the names "." and ".." as we don't want to recurse on them. */
            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
                continue;
            }

            const size_t len = path_len + strlen(p->d_name) + 2;
            StackBuffer<PATH_MAX> buffer(len);

            if (buffer) {
                struct stat statbuf;
                snprintf(buffer, len, "%s/%s", dir.constData(), p->d_name);
                if (!::stat(buffer, &statbuf)) {
                    if (S_ISDIR(statbuf.st_mode)) {
                        Path::rmdir(Path(buffer));
                    } else {
                        unlink(buffer);
                    }
                }
            }
        }
        closedir(d);
    }
    return ::rmdir(dir.constData()) == 0;
#endif
}

static void visitorWrapper(Path path, const std::function<Path::VisitResult(const Path &path)> &callback, Set<Path> &seen)
{
    if (!seen.insert(path.resolved())) {
        return;
    }

#ifdef _WIN32
    _WDIR *d = _wopendir(Utf8To16(path.c_str()));
    if(!d)
        return;
#else
    DIR *d = opendir(path.constData());
    if (!d)
        return;
#endif

    union {
        char buf[PATH_MAX + sizeof(dirent) + 1];
        dirent dbuf;
    };


    if (!path.endsWith('/'))
        path.append('/');
    const size_t s = path.size();
    path.reserve(s + 128);
    List<String> recurseDirs;
#ifdef _WIN32
    _wdirent *p;
    while ((p = _wreaddir(d))) {
        Utf16To8 u8Data(p->d_name);
        const char *d_name = u8Data;
#else
    while (dirent *p = readdir(d)) {
        const char *d_name = p->d_name;
#endif
        if (!strcmp(d_name, ".") || !strcmp(d_name, ".."))
            continue;
        bool isDir = false;
        path.truncate(s);
        path.append(d_name);
#if !defined(_WIN32) && defined(_DIRENT_HAVE_D_TYPE) && defined(_BSD_SOURCE)
        if (p->d_type == DT_DIR) {
            isDir = true;
            path.append('/');
        }
#else
        isDir = path.isDir();
        if (isDir)
            path.append('/');
#endif
        switch (callback(path)) {
        case Path::Abort:
            p = nullptr;
            break;
        case Path::Recurse:
            if (isDir)
                recurseDirs.append(d_name);
            break;
        case Path::Continue:
            break;
        }
    }
#ifdef _WIN32
    _wclosedir(d);
#else
    closedir(d);
#endif
    const size_t count = recurseDirs.size();
    for (size_t i=0; i<count; ++i) {
        path.truncate(s);
        path.append(recurseDirs.at(i));
        visitorWrapper(path, callback, seen);
    }
}

void Path::visit(const std::function<VisitResult(const Path &path)> &callback) const
{
    if (!callback || !isDir())
        return;
    Set<Path> seenDirs;
    visitorWrapper(*this, callback, seenDirs);
}

Path Path::followLink(bool *ok) const
{
#ifndef _WIN32  //no symlinks on windows
    if (isSymLink()) {
        char buf[PATH_MAX];
        const int w = readlink(constData(), buf, sizeof(buf) - 1);
        if (w != -1) {
            if (ok)
                *ok = true;
            buf[w] = '\0';
            return buf;
        }
    }
#endif

    // could not follow the link (maybe because it's not a link)

    if (ok)
        *ok = false;

    return *this;
}

size_t Path::readAll(char *&buf, size_t max) const
{
    FILE *f = fopen(constData(), "r");
    buf = nullptr;
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    int size = ftell(f);
    if (max > 0 && max < static_cast<size_t>(size))
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

String Path::readAll(size_t max) const
{
    FILE *f = fopen(constData(), "r");
    if (!f)
        return String();
    const String ret = Rct::readAll(f, max);
    fclose(f);
    return ret;
}

bool Path::write(const Path &path, const String &data, WriteMode mode)
{
    FILE *f = fopen(path.constData(), mode == Overwrite ? "w" : "a");
    if (!f)
        return false;
    const size_t ret = fwrite(data.constData(), sizeof(char), data.size(), f);
    fclose(f);
    return ret == data.size();
}

bool Path::write(const String &data, WriteMode mode) const
{
    return Path::write(*this, data, mode);
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
    char *pwd = getenv("PWD");
    if (!pwd)
        pwd = getcwd(buf, sizeof(buf));
    if (pwd) {
        Path ret(pwd);
        if (!ret.endsWith('/'))
            ret.append('/');
        return ret;
    }
    return Path();
}
List<Path> Path::files(unsigned int filter, size_t max, bool recurse) const
{
    assert(max != 0);

    List<Path> paths;
    visit([filter, &max, recurse, &paths](const Path &path) {
            if (max > 0)
                --max;
            if (path.type() & filter) {
                paths.append(path);
            }
            if (!max)
                return Path::Abort;
            return recurse ? Path::Recurse : Path::Continue;
        });
    return paths;
}

uint64_t Path::lastModifiedMs() const
{
    bool ok;
    struct stat st = stat(&ok);
    if (!ok)
        return 0;

#ifdef HAVE_STATMTIM
    return st.st_mtim.tv_sec * static_cast<uint64_t>(1000) + st.st_mtim.tv_nsec / static_cast<uint64_t>(1000000);
#else
    return st.st_mtime * static_cast<uint64_t>(1000);
#endif
}
const char *Path::typeName(Type type)
{
    switch (type) {
    case Invalid: return "Invalid";
    case File: return "File";
    case Directory: return "Directory";
    case CharacterDevice: return "CharacterDevice";
    case BlockDevice: return "BlockDevice";
    case NamedPipe: return "NamedPipe";
    case Socket: return "Socket";
    default:
        break;
    }
    return "";
}

struct stat Path::stat(bool *f_ok) const
{
    struct stat st;
#ifdef _WIN32
    if (::wstat(Utf8To16(c_str()), &st) == -1) {
#else
    if (::stat(constData(), &st) == -1) {
#endif
        memset(&st, 0, sizeof(st));
        if (f_ok) *f_ok = false;
    } else if (f_ok) {
        *f_ok = true;
    }
    return st;
}

String Path::name() const
{
    if (endsWith('/')) {
        const size_t secondLastSlash = lastIndexOf('/', size() - 2);
        if (secondLastSlash != String::npos) {
            return mid(secondLastSlash + 1, size() - secondLastSlash - 2);
        }
        return String();
    } else {
        return fileName();
    }
}

void Path::replaceBackslashes()
{
    std::size_t start = 0;

    // don't replace \\ at the beginning (network path)
    if(size() >= 2 && (*this)[0] == '\\' && (*this)[1] == '\\')
    {
        start = 2;
    }

    for(std::size_t i=start; i<size(); i++)
    {
        if((*this)[i] == '\\') (*this)[i] = '/';
    }
}
