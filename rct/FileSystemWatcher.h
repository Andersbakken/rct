#ifndef FileSystemWatcher_h
#define FileSystemWatcher_h

#include <stdint.h>
#include <mutex>

#include <rct/rct-config.h>
#include <rct/Map.h>
#include <rct/Path.h>
#include <rct/Set.h>
#include <rct/SignalSlot.h>
#include <rct/Timer.h>

#ifdef HAVE_FSEVENTS
#include <CoreServices/CoreServices.h>
#ifdef check
#undef check
#endif
class WatcherData;
#elif defined(HAVE_CHANGENOTIFICATION)
class WatcherData;
#elif defined(HAVE_KQUEUE)
#elif defined(HAVE_INOTIFY)
#else
#error no filesystemwatcher backend
#endif


class FileSystemWatcher
{
public:
    struct Options {
        Options()
            : removeDelay(1000)
        {}
        int removeDelay;
    };
    FileSystemWatcher(const Options &option = Options());
    ~FileSystemWatcher();

    bool watch(const Path &path);
    bool unwatch(const Path &path);
    Signal<std::function<void(const Path &)> > &removed() { return mRemoved; }
    Signal<std::function<void(const Path &)> > &added() { return mAdded; }
    Signal<std::function<void(const Path &)> > &modified() { return mModified; }
    void clear();
#if defined(HAVE_FSEVENTS) || defined(HAVE_CHANGENOTIFICATION)
    Set<Path> watchedPaths() const;
#else
    Set<Path> watchedPaths() const { return mWatchedByPath.keys().toSet(); } // ### slow
#endif
private:
    void init();
    void shutdown();
#if defined(HAVE_FSEVENTS) || defined(HAVE_CHANGENOTIFICATION)
    WatcherData* mWatcher;
    friend class WatcherData;
    void pathsAdded(const Set<Path>& paths);
    void pathsRemoved(const Set<Path>& paths);
    void pathsModified(const Set<Path>& paths);
#if !defined(HAVE_CHANGENOTIFICATION) // only for HAVE_FSEVENTS
    bool isWatching(const Path& path) const;
#endif
#else
    void notifyReadyRead();
    int mFd;
    Map<Path, int> mWatchedByPath;
    Map<int, Path> mWatchedById;

#ifdef HAVE_KQUEUE
    Map<Path, uint64_t> mTimes;
    static Path::VisitResult scanFiles(const Path& path, void* userData);
    static Path::VisitResult updateFiles(const Path& path, void* userData);

    bool isWatching(const Path& path) const;
#endif
#endif
    std::mutex mMutex;
    Signal<std::function<void(const Path&)> > mRemoved, mModified, mAdded;

    enum Type {
        Add = 0x1,
        Remove = 0x2,
        Modified = 0x4
    };
    void add(Type type, const Path &path)
    {
        switch (type) {
        case Add:
            if (mRemovedPaths.remove(path)) {
                mModifiedPaths.insert(path);
            } else {
                mAddedPaths.insert(path);
            }
            break;
        case Remove:
            if (!mAddedPaths.remove(path))
                mRemovedPaths.insert(path);
            break;
        case Modified:
            mModifiedPaths.insert(path);
            break;
        }
    }
    const Options mOptions;
    Set<Path> mAddedPaths, mRemovedPaths, mModifiedPaths;
    Timer mTimer;
    void processChanges();
    void processChanges(unsigned int types);
};

#endif
