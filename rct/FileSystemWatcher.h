#ifndef FileSystemWatcher_h
#define FileSystemWatcher_h

#include "rct-config.h"
#include <rct/Path.h>
#include <rct/Map.h>
#include <rct/Set.h>
#include <rct/SignalSlot.h>
#include <stdint.h>
#include <mutex>
#ifdef HAVE_FSEVENTS
#include <CoreServices/CoreServices.h>
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
    FileSystemWatcher();
    ~FileSystemWatcher();

    bool watch(const Path &path);
    bool unwatch(const Path &path);
    Signal<std::function<void(const Path &)> > &removed() { return mRemoved; }
    Signal<std::function<void(const Path &)> > &added() { return mAdded; }
    Signal<std::function<void(const Path &)> > &modified() { return mModified; }
    void clear();
#if defined(HAVE_FSEVENTS) or defined(HAVE_CHANGENOTIFICATION)
    Set<Path> watchedPaths() const;
#else
    Set<Path> watchedPaths() const { return mWatchedByPath.keys().toSet(); } // ### slow
#endif
private:
#if defined(HAVE_FSEVENTS) or defined(HAVE_CHANGENOTIFICATION)
    WatcherData* mWatcher;
    friend class WatcherData;
    void pathsAdded(const Set<Path>& paths);
    void pathsRemoved(const Set<Path>& paths);
    void pathsModified(const Set<Path>& paths);
#if !defined(HAVE_CHANGENOTIFICATION) // only for HAVE_FSEVENTS
    bool isWatching(const Path& path) const;
#endif
#else
    std::mutex mMutex;
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
    Signal<std::function<void(const Path&)> > mRemoved, mModified, mAdded;
};
#endif



















