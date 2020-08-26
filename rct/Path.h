#ifndef Path_h
#define Path_h

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <functional>
#include <string>
#ifndef _WIN32
#  include <sys/mman.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <rct/String.h>

/**
 * A path is a special string that represents a file system path. This may be a
 * directory or a file. A path can be absolute or relative.
 *
 * Note: On windows, all paths use forward slashes (/) as path separator, just
 * like on unix.
 * Paths that are created with backslashes are automatically converted.
 */
class Path : public String
{
public:
    Path(const Path &other)
        : String(other)
    {}
    Path(Path &&other)
        : String(std::move(other))
    {}

    Path(const String &other)
        : String(other)
    {
#ifdef _WIN32
        replaceBackslashes();
#endif
    }
    Path(String &&other)
        : String(std::move(other))
    {
#ifdef _WIN32
        replaceBackslashes();
#endif
    }

    Path(const char *path)
        : String(path)
    {
#ifdef _WIN32
        replaceBackslashes();
#endif
    }
    Path(const char *path, size_t len)
        : String(path, len)
    {
#ifdef _WIN32
        replaceBackslashes();
#endif
    }
    Path() {}
    Path &operator=(const Path &other)
    {
        String::operator=(other);
        return *this;
    }
    Path &operator=(Path &&other)
    {
        String::operator=(std::move(other));
        return *this;
    }

    Path &operator=(const String &other)
    {
        String::operator=(other);
#ifdef _WIN32
        replaceBackslashes();
#endif
        return *this;
    }

    Path &operator=(String &&other)
    {
        String::operator=(std::move(other));
#ifdef _WIN32
        replaceBackslashes();
#endif
        return *this;
    }


    Path &operator=(const char *path)
    {
        String::operator=(path);
#ifdef _WIN32
        replaceBackslashes();
#endif
        return *this;
    }

    bool isSameFile(const Path &other) const
    {
        return Path::resolved(*this).String::operator==(Path::resolved(other));
    }

    enum Type {
        Invalid = 0x00,
        File = 0x01,
        Directory = 0x02,
        CharacterDevice = 0x04,
        BlockDevice = 0x08,
        NamedPipe = 0x10,
        Socket = 0x40,
        All = File|Directory|CharacterDevice|BlockDevice|NamedPipe|Socket
    };

    inline bool exists() const { return type() != Invalid; }
    inline bool isDir() const { return type() == Directory; }
    inline bool isFile() const { return type() == File; }
    inline bool isExecutable() const { return !access(constData(), X_OK); }
    inline bool isSocket() const { return type() == Socket; }
    bool isAbsolute() const;
    static const char *typeName(Type type);
    bool isSymLink() const;
    Path followLink(bool *ok = nullptr) const;
    String name() const;
    const char *fileName(size_t *len = nullptr) const;
    const char *extension(size_t *len = nullptr) const;
    static bool exists(const Path &path) { return path.exists(); }
    enum MkDirMode {
        Single,
        Recursive
    };

    /**
     * Create the directory that is represented by this path.
     *
     * @param permissions ignored on windows.
     * @return true if the directory was created or already existed
     */
    static bool mkdir(const Path &path,
                      MkDirMode mode = Single,
                      mode_t permissions = S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
    bool mkdir(MkDirMode mode = Single,
               mode_t permissions = S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) const;
    static bool rm(const Path &file);

    /**
     * Recursively delete a directory
     */
    static bool rmdir(const Path& dir);
    static Path home();

    inline Path ensureTrailingSlash() const
    {
        if (!isEmpty() && !endsWith('/'))
            return *this + '/';
        return *this;
    }

    bool rm() const { return Path::rm(*this); }
    bool mksubdir(const String &subdir) const;
    bool isSource() const;
    static bool isSource(const char *extension);
    bool isSystem() const { return Path::isSystem(constData()); }
    static bool isSystem(const char *path);
    bool isHeader() const;
    static bool isHeader(const char *extension);

    /// Only works if the path is absolute
    Path parentDir() const;

    Type type() const;
    mode_t mode() const;
    enum ResolveMode {
        RealPath,
        MakeAbsolute
    };
    static bool realPathEnabled() { return sRealPathEnabled; }
    static void setRealPathEnabled(bool enabled) { sRealPathEnabled = enabled; }
    Path resolved(ResolveMode mode = RealPath, const Path &cwd = Path(), bool *ok = nullptr) const;
    bool resolve(ResolveMode mode = RealPath, const Path &cwd = Path(), bool *changed = nullptr);
    size_t canonicalize(bool *changed = nullptr);
    Path canonicalized() const;
    static Path canonicalized(const Path &path);
    time_t lastModified() const; // returns time_t ... no shit
    time_t lastAccess() const;
    bool setLastModified(time_t lastModified) const;
    uint64_t lastModifiedMs() const;

    struct stat stat(bool *ok = nullptr) const;

    int64_t fileSize() const;
    static Path resolved(const String &path, ResolveMode mode = RealPath, const Path &cwd = Path(), bool *ok = nullptr);
    static Path canonicalized(const String &path);
    static Path pwd();
    size_t readAll(char *&, size_t max = -1) const;
    String readAll(size_t max = -1) const;

    bool touch() const
    {
        if (FILE *f = fopen(constData(), "a")) {
            fclose(f);
            return true;
        }
        return false;
    }

    enum WriteMode {
        Overwrite,
        Append
    };
    bool write(const String& data, WriteMode mode = Overwrite) const;
    static bool write(const Path& path, const String& data, WriteMode mode = Overwrite);

    Path toTilde() const;

    enum VisitResult {
        Abort,
        Continue,
        Recurse
    };
    void visit(const std::function<VisitResult(const Path &path)> &callback) const;
    List<Path> files(unsigned int filter = All, size_t max = String::npos, bool recurse = false) const;

    static bool sRealPathEnabled;

    /// ';' on windows, ':' on unix
    static const char ENV_PATH_SEPARATOR;

    /**
     * For windows. Replace backslashes (\) in the path by forward slashes (/).
     */
    void replaceBackslashes();
};

namespace std
{
template <> struct hash<Path> : public unary_function<Path, size_t>
{
    size_t operator()(const Path& value) const
    {
        std::hash<std::string> h;
        return h(value);
    }
};
}

#endif
