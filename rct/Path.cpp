#include "Path.h"
#include "Log.h"
#include "Rct.h"
#include "rct-config.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fts.h>
#include <wordexp.h>

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

bool Path::resolve(ResolveMode mode, const Path &cwd, bool *changed)
{
    if (changed)
        *changed = false;
    if (startsWith('~')) {
        wordexp_t exp_result;
        wordexp(constData(), &exp_result, 0);
        operator=(exp_result.we_wordv[0]);
        wordfree(&exp_result);
    }
    if (mode == MakeAbsolute) {
        if (isAbsolute())
            return true;
        const Path copy = (cwd.isEmpty() ? Path::pwd() : cwd.ensureTrailingSlash()) + *this;
        if (copy.exists()) {
            if (changed)
                *changed = true;
            operator=(copy);
            return true;
        }
        return false;
    }

    if (!cwd.isEmpty() && !isAbsolute()) {
        Path copy = cwd + '/' + *this;
        if (copy.resolve(RealPath, Path(), changed)) {
            operator=(copy);
            return true;
        }
    }

    {
        char buffer[PATH_MAX + 2];
        if (realpath(constData(), buffer)) {
            if (isDir()) {
                const int len = strlen(buffer);
                assert(buffer[len] != '/');
                buffer[len] = '/';
                buffer[len + 1] = '\0';
            }
            if (changed && strcmp(buffer, constData()))
                *changed = true;
            String::operator=(buffer);
            return true;
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

const char * Path::extension(int *len) const
{
    const int s = size();
    int dot = s - 1;
    const char *data = constData();
    while (dot >= 0) {
        switch (data[dot]) {
        case '.':
            if (len)
                *len = s - (dot + 1);
            return data + dot + 1;
        case '/':
            break;
        default:
            break;
        }
        --dot;
    }
    if (len)
        *len = 0;
    return 0;
}

bool Path::isSource(const char *ext)
{
    const char *sources[] = { "c", "cc", "cpp", "cxx", "moc", "mm", "m", 0 };
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

bool Path::mkdir(MkDirMode mkdirMode, mode_t permissions) const
{
    return Path::mkdir(*this, mkdirMode, permissions);
}

bool Path::rm(const Path &file)
{
    return !unlink(file.constData());
}

static inline Path::Type ftsType(uint16_t type)
{
    if (type & (FTS_F|FTS_SL|FTS_SLNONE))
        return Path::File;
    if (type & FTS_DP) // omitting FTS_D on purpose here
        return Path::Directory;
    return Path::Invalid;
}

bool Path::rmdir(const Path& dir)
{
    if (!dir.isDir())
        return false;
    // hva slags drittapi er dette?
    char* const dirs[2] = { const_cast<char*>(dir.constData()), 0 };
    FTS* fdir = fts_open(dirs, FTS_NOCHDIR, 0);
    if (!fdir)
        return false;
    FTSENT *node;
    while ((node = fts_read(fdir))) {
        if (node->fts_level > 0 && node->fts_name[0] == '.') {
            fts_set(fdir, node, FTS_SKIP);
        } else {
            switch (ftsType(node->fts_info)) {
            case File:
                unlink(node->fts_path);
                break;
            case Directory:
                ::rmdir(node->fts_path);
                break;
            default:
                break;
            }
        }
    }
    fts_close(fdir);
    return true;
}

static void visitorWrapper(Path path, Path::VisitCallback callback, Set<Path> &seen, void *userData)
{
    if (!seen.insert(path.resolved())) {
        return;
    }
    DIR *d = opendir(path.constData());
    if (!d)
        return;

    char buf[PATH_MAX + sizeof(dirent) + 1];
    dirent *dbuf = reinterpret_cast<dirent*>(buf);

    dirent *p;
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
        case Path::Abort:
            p = 0;
            break;
        case Path::Recurse:
            if (path.isDir())
                recurseDirs.append(p->d_name);
            break;
        case Path::Continue:
            break;
        }
    }
    closedir(d);
    const int count = recurseDirs.size();
    for (int i=0; i<count; ++i) {
        path.truncate(s);
        path.append(recurseDirs.at(i));
        visitorWrapper(path, callback, seen, userData);
    }
}

void Path::visit(VisitCallback callback, void *userData) const
{
    if (!callback || !isDir())
        return;
    Set<Path> seenDirs;
    visitorWrapper(*this, callback, seenDirs, userData);
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

bool Path::write(const Path& path, const String& data, WriteMode mode)
{
    FILE* f = fopen(path.constData(), mode == Overwrite ? "w" : "a");
    if (!f)
        return false;
    const int ret = fwrite(data.constData(), sizeof(char), data.size(), f);
    fclose(f);
    return ret == data.size();
}

bool Path::write(const String& data, WriteMode mode) const
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
        return 0;
    }
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
