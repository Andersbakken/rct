#ifndef FileSystemWatcher_h
#define FileSystemWatcher_h

#include "rct-config.h"
#include <rct/Path.h>
#include <rct/Map.h>
#include <rct/Mutex.h>
#include <rct/SignalSlot.h>
#include <rct/Tr1.h>
#include <stdint.h>
#ifdef HAVE_FSEVENTS
#include <rct/CoreServices/CoreServices.h>
class WatcherThread;
class WatcherReceiver;
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
#ifdef HAVE_FSEVENTS
    Set<Path> watchedPaths() const;
#else
    Set<Path> watchedPaths() const { return mWatchedByPath.keys().toSet(); } // ### slow
#endif
private:
#ifdef HAVE_FSEVENTS
    WatcherThread* mWatcher;
    WatcherReceiver* mReceiver;
    friend class WatcherReceiver;
#else
    Mutex mMutex;
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
