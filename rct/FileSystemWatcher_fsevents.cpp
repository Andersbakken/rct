#include "FileSystemWatcher.h"

#include <CoreFoundation/CoreFoundation.h>
#include <errno.h>
#include <mutex>
#include <string.h>
#include <thread>

#include "EventLoop.h"
#include "Log.h"
#include "Rct.h"
#include "StackBuffer.h"
#include "rct/rct-config.h"

class WatcherData : public std::enable_shared_from_this<WatcherData>
{
public:
    WatcherData(FileSystemWatcher *fsw);
    ~WatcherData();

    void clear();
    void watch(const Path &path);
    bool unwatch(const Path &path);
    void waitForStarted();
    Set<Path> watchedPaths() const;

    mutable std::mutex mutex;

    Set<Path> paths;

    enum Flags
    {
        Start = 0x1,
        Stop  = 0x2,
        Clear = 0x4
    };

    int flags;

    FileSystemWatcher *watcher;
    dispatch_queue_t queue;
    FSEventStreamRef fss;
    FSEventStreamEventId since;
    std::condition_variable waiter;
    static void notifyCallback(ConstFSEventStreamRef, void *, size_t, void *, const FSEventStreamEventFlags[],
                               const FSEventStreamEventId[]);
    void perform();
};

WatcherData::WatcherData(FileSystemWatcher *fsw)
    : flags(0)
    , watcher(fsw)
    , fss(0)
{
    // ### is this right?
    since = kFSEventStreamEventIdSinceNow;
    queue = dispatch_queue_create("com.rtags.fsevents", DISPATCH_QUEUE_SERIAL);

    {
        std::lock_guard<std::mutex> locker(mutex);
        flags |= Start;
        waiter.notify_one();
    }
}

WatcherData::~WatcherData()
{
    dispatch_sync(queue, ^{
        if (fss) {
            FSEventStreamStop(fss);
            FSEventStreamInvalidate(fss);
            FSEventStreamRelease(fss);
            fss = nullptr;
        }
    });

    dispatch_release(queue);
}

void WatcherData::waitForStarted()
{
    std::unique_lock<std::mutex> locker(mutex);
    while (!(flags & Start)) {
        waiter.wait(locker);
    }
}

void WatcherData::watch(const Path &path)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        assert(!paths.contains(path));
        paths.insert(path);
    }

    auto self = shared_from_this();
    dispatch_async(queue, ^{
        self->perform();
    });
}

bool WatcherData::unwatch(const Path &path)
{
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto p = paths.find(path);
        if (p == paths.end())
            return false;

        paths.erase(p);
        found = true;
    }

    if (found) {
        auto self = shared_from_this();
        dispatch_async(queue, ^{
            self->perform();
        });
    }
    return found;
}

void WatcherData::clear()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        flags |= Clear;
        paths.clear();
    }

    auto self = shared_from_this();
    dispatch_async(queue, ^{
        self->perform();
    });
}

Set<Path> WatcherData::watchedPaths() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return paths;
}

void WatcherData::perform()
{
    std::unique_lock<std::mutex> locker(mutex);
    if (flags & Stop) {
        if (fss) {
            FSEventStreamStop(fss);
            FSEventStreamInvalidate(fss);
        }
        return;
    } else if (flags & Clear) {
        flags &= ~Clear;

        if (fss) {
            FSEventStreamStop(fss);
            FSEventStreamInvalidate(fss);
            FSEventStreamRelease(fss);
            fss = 0;
        }

        // We might have paths added since the clear operation was inititated
        if (paths.empty())
            return;
    }

    // ### might make sense to have multiple streams instead of recreating one for each change
    // ### and then merge them if the stream count reaches a given treshold

    const int pathSize = paths.size();
    FSEventStreamRef newfss = 0;

    if (pathSize) {
        StackBuffer<1024, CFStringRef> refs(pathSize);
        int i = 0;
        const Set<Path> copy = paths;
        for (const Path &path : copy) {
            refs[i++] = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, path.c_str(), kCFStringEncodingUTF8, kCFAllocatorNull);
        }

        // don't need to hold the mutex beyond this point
        locker.unlock();

        CFArrayRef list = CFArrayCreate(kCFAllocatorDefault, reinterpret_cast<const void **>(&refs[0]), pathSize, &kCFTypeArrayCallBacks);

        for (int j = 0; j < i; ++j)
            CFRelease(refs[j]);

        FSEventStreamContext ctx = { 0, this, 0, 0, 0 };
        newfss = FSEventStreamCreate(kCFAllocatorDefault, notifyCallback, &ctx, list, since, .1, kFSEventStreamCreateFlagIgnoreSelf | kFSEventStreamCreateFlagFileEvents);

        CFRelease(list);
    }

    if (!newfss)
        return;

    if (fss) {
        FSEventStreamStop(fss);
        FSEventStreamInvalidate(fss);
        FSEventStreamRelease(fss);
    }

    fss = newfss;

    FSEventStreamSetDispatchQueue(fss, queue);
    FSEventStreamStart(fss);
}

void WatcherData::notifyCallback(ConstFSEventStreamRef streamRef, void *clientCallBackInfo, size_t numEvents, void *eventPaths,
                                 const FSEventStreamEventFlags eventFlags[], const FSEventStreamEventId eventIds[])
{
    (void)eventIds;
    WatcherData *watcher = static_cast<WatcherData *>(clientCallBackInfo);
    std::lock_guard<std::mutex> locker(watcher->mutex);
    watcher->since = FSEventStreamGetLatestEventId(streamRef);
    char **paths   = reinterpret_cast<char **>(eventPaths);

    FileSystemWatcher *fsWatcher = watcher->watcher;
    {
        std::lock_guard<std::mutex> l(fsWatcher->mMutex);
        for (size_t i = 0; i < numEvents; ++i) {
            const FSEventStreamEventFlags flags = eventFlags[i];
            if (flags & kFSEventStreamEventFlagHistoryDone)
                continue;
            if (flags & kFSEventStreamEventFlagItemIsFile) {
                const Path path(paths[i]);
                if (flags & kFSEventStreamEventFlagItemCreated) {
                    fsWatcher->add(FileSystemWatcher::Add, path);
                }
                if (flags & kFSEventStreamEventFlagItemRemoved) {
                    fsWatcher->add(FileSystemWatcher::Remove, path);
                }
                if (flags & kFSEventStreamEventFlagItemRenamed) {
                    if (path.isFile()) {
                        fsWatcher->add(FileSystemWatcher::Add, path);
                    } else {
                        fsWatcher->add(FileSystemWatcher::Remove, path);
                    }
                }
                if (flags & (kFSEventStreamEventFlagItemModified | kFSEventStreamEventFlagItemInodeMetaMod)) {
                    fsWatcher->add(FileSystemWatcher::Modified, path);
                }
            }
        }
    }

    std::weak_ptr<WatcherData> that = watcher->shared_from_this();
    EventLoop::eventLoop()->callLater([that]
                                      {
                                          if (std::shared_ptr<WatcherData> watcherData = that.lock()) {
                                              watcherData->watcher->processChanges();
                                          }
                                      });
}

void FileSystemWatcher::init()
{
    mWatcher.reset(new WatcherData(this));
    mWatcher->waitForStarted();
}

void FileSystemWatcher::shutdown()
{
    mWatcher.reset();
}

void FileSystemWatcher::clear()
{
    mWatcher->clear();
}

Set<Path> FileSystemWatcher::watchedPaths() const
{
    return mWatcher->watchedPaths();
}

bool FileSystemWatcher::isWatching(const Path &p) const
{
    if (!p.endsWith('/'))
        return isWatching(p + '/');

    std::lock_guard<std::mutex> lock(mWatcher->mutex);
    return mWatcher->paths.contains(p);
}

bool FileSystemWatcher::watch(const Path &p)
{
    Path path = p;
    assert(!path.empty());
    const Path::Type type = path.type();
    switch (type) {
        case Path::File:
            path = path.parentDir();
            [[fallthrough]];
        case Path::Directory:
            break;
        default:
            error("FileSystemWatcher::watch() '%s' doesn't seem to be watchable", path.c_str());
            return false;
    }

    if (!path.endsWith('/'))
        path += '/';

    if (isWatching(path))
        return false;

    mWatcher->watch(path);

    return true;
}

bool FileSystemWatcher::unwatch(const Path &p)
{
    Path path = p;
    if (path.isFile())
        path = path.parentDir();

    if (!path.endsWith('/'))
        path += '/';

    return mWatcher->unwatch(path);
}

void FileSystemWatcher::pathsAdded(const Set<Path> &paths)
{
    for (const Path &path : paths) {
        mAdded(path);
    }
}

void FileSystemWatcher::pathsRemoved(const Set<Path> &paths)
{
    for (const Path &path : paths) {
        mRemoved(path);
    }
}

void FileSystemWatcher::pathsModified(const Set<Path> &paths)
{
    for (const Path &path : paths) {
        mModified(path);
    }
}
