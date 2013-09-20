#ifndef FileSystemWatcher_h
#define FileSystemWatcher_h

#include "rct-config.h"
#include <rct/Path.h>
#include <rct/Map.h>
#include <rct/SignalSlot.h>
#include <stdint.h>
#include <mutex>
#ifdef HAVE_FSEVENTS
#include <CoreServices/CoreServices.h>
class WatcherData;
#elif defined(HAVE_KQUEUE)
#elif defined(HAVE_INOTIFY)
#elif defined(HAVE_FAM)
#include "fam.h"
#else
#error no filesystemwatcher backend
#endif


class FileSystemWatcher
{
public:
    FileSystemWatcher();
    ~FileSystemWatcher();

    bool watch(const Path &path);
    bool unwatch(const Path &path);
    Signal<std::function<void(const Path &)> > &removed() { return mRemoved; }
    Signal<std::function<void(const Path &)> > &added() { return mAdded; }
    Signal<std::function<void(const Path &)> > &modified() { return mModified; }
    void clear();
#ifdef HAVE_FSEVENTS
    Set<Path> watchedPaths() const;
#else
    Set<Path> watchedPaths() const { return mWatchedByPath.keys().toSet(); } // ### slow
#endif
private:
#ifdef HAVE_FSEVENTS
    WatcherData* mWatcher;
    friend class WatcherData;
    void pathsAdded(const Set<Path>& paths);
    void pathsRemoved(const Set<Path>& paths);
    void pathsModified(const Set<Path>& paths);
    bool isWatching(const Path& path) const;
#else
    std::mutex mMutex;
    void notifyReadyRead();
    int mFd;
    Map<Path, int> mWatchedByPath;
    Map<int, Path> mWatchedById;

#ifdef HAVE_FAM
    FAMConnection mFAMCon;
    void checkFAMEvents(int);
    bool isFAMEventPending();
#endif

#ifdef HAVE_KQUEUE
    Map<Path, uint64_t> mTimes;
    static Path::VisitResult scanFiles(const Path& path, void* userData);
    static Path::VisitResult updateFiles(const Path& path, void* userData);

    bool isWatching(const Path& path) const;
#endif
#endif
    Signal<std::function<void(const Path&)> > mRemoved, mModified, mAdded;
};
#endif



















