#ifndef Path_h
#define Path_h

#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <rct/Set.h>
#include <rct/String.h>
#include <string>

class Path : public String
{
public:
    Path(const Path &other)
        : String(other)
    {}
    Path(const String &other)
        : String(other)
    {}
    Path(const char *path)
        : String(path)
    {}
    Path(const char *path, int size)
        : String(path, size)
    {}
    Path() {}
    Path &operator=(const Path &other)
    {
        String::operator=(other);
        return *this;
    }
    Path &operator=(const String &other)
    {
        String::operator=(other);
        return *this;
    }

    Path &operator=(const char *path)
    {
        String::operator=(path);
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
    inline bool isSocket() const { return type() == Socket; }
    inline bool isAbsolute() const { return (!isEmpty() && at(0) == '/'); }
    static const char *typeName(Type type);
    bool isSymLink() const;
    Path followLink(bool *ok = 0) const;
    const char *fileName(int *len = 0) const;
    const char *extension(int *len = 0) const;
    static bool exists(const Path &path) { return path.exists(); }
    enum MkDirMode {
        Single,
        Recursive
    };
    static bool mkdir(const Path &path,
                      MkDirMode mode = Single,
                      mode_t permissions = S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
    bool mkdir(MkDirMode mode = Single,
               mode_t permissions = S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) const;
    static bool rm(const Path &file);
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
    Path parentDir() const;
    Type type() const;
    mode_t mode() const;
    enum ResolveMode {
        RealPath,
        MakeAbsolute
    };
    Path resolved(ResolveMode mode = RealPath, const Path &cwd = Path(), bool *ok = 0) const;
    bool resolve(ResolveMode mode = RealPath, const Path &cwd = Path(), bool *changed = 0);
    int canonicalize();
    Path canonicalized() const;
    static Path canonicalized(const Path &path);
    time_t lastModified() const; // returns time_t ... no shit
    uint64_t lastModifiedMs() const;

    int64_t fileSize() const;
    static Path resolved(const String &path, ResolveMode mode = RealPath, const Path &cwd = Path(), bool *ok = 0);
    static Path canonicalized(const String &path);
    static Path pwd();
    int readAll(char *&, int max = -1) const;
    String readAll(int max = -1) const;

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
    typedef VisitResult (*VisitCallback)(const Path &path, void *userData);
    void visit(VisitCallback callback, void *userData) const;

    List<Path> files(unsigned filter = All, int max = -1, bool recurse = false) const;
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
