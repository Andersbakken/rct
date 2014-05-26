#include "FileSystemWatcher.h"
#include "EventLoop.h"
#include "Log.h"
#include "rct-config.h"
#include <string.h>
#include <errno.h>
#include <mutex>
#include <thread>
#include <CoreFoundation/CoreFoundation.h>

class WatcherData
{
public:
    WatcherData(FileSystemWatcher* fsw);
    ~WatcherData();

    void clear();
    void watch(const Path& path);
    bool unwatch(const Path& path);
    void waitForStarted();
    Set<Path> watchedPaths() const;

    mutable std::mutex mutex;

    Set<Path> paths;

    enum Flags {
        Start = 0x1,
        Stop = 0x2,
        Clear = 0x4
    };
    int flags;

    FileSystemWatcher* watcher;
    CFRunLoopRef loop;
    CFRunLoopSourceRef source;
    FSEventStreamRef fss;
    FSEventStreamEventId since;
    std::thread thread;
    std::condition_variable waiter;
    static void notifyCallback(ConstFSEventStreamRef, void*, size_t, void *,
                               const FSEventStreamEventFlags[],
                               const FSEventStreamEventId[]);
    static void perform(void* thread);
};

WatcherData::WatcherData(FileSystemWatcher* fsw)
    : flags(0), watcher(fsw), fss(0)
{
    // ### is this right?
    since = kFSEventStreamEventIdSinceNow;
    CFRunLoopSourceContext ctx;
    memset(&ctx, '\0', sizeof(CFRunLoopSourceContext));
    ctx.info = this;
    ctx.perform = perform;
    source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &ctx);

    thread = std::thread([=]() {
            {
                std::lock_guard<std::mutex> locker(mutex);
                loop = CFRunLoopGetCurrent();
                CFRunLoopAddSource(loop, source, kCFRunLoopCommonModes);
                flags |= Start;
                waiter.notify_one();
            }
            CFRunLoopRun();
        });
}

WatcherData::~WatcherData()
{
    // stop the thread;
    {
        std::lock_guard<std::mutex> locker(mutex);
        flags |= Stop;
        CFRunLoopSourceSignal(source);
        CFRunLoopWakeUp(loop);
    }
    thread.join();

    CFRunLoopSourceInvalidate(source);
    CFRelease(source);
}

void WatcherData::waitForStarted()
{
    std::unique_lock<std::mutex> locker(mutex);
    while (!(flags & Start)) {
        waiter.wait(locker);
    }
}

void WatcherData::watch(const Path& path)
{
    std::lock_guard<std::mutex> lock(mutex);

    assert(!paths.contains(path));

    paths.insert(path);
    CFRunLoopSourceSignal(source);
    CFRunLoopWakeUp(loop);
}

bool WatcherData::unwatch(const Path& path)
{
    std::lock_guard<std::mutex> lock(mutex);

    auto p = paths.find(path);
    if (p == paths.end())
        return false;

    paths.erase(p);
    CFRunLoopSourceSignal(source);
    CFRunLoopWakeUp(loop);
    return true;
}

void WatcherData::clear()
{
    std::lock_guard<std::mutex> lock(mutex);

    flags |= Clear;
    paths.clear();
    CFRunLoopSourceSignal(source);
    CFRunLoopWakeUp(loop);
}

Set<Path> WatcherData::watchedPaths() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return paths;
}

void WatcherData::perform(void* thread)
{
    WatcherData* watcher = static_cast<WatcherData*>(thread);
    std::unique_lock<std::mutex> locker(watcher->mutex);
    if (watcher->flags & Stop) {
        if (watcher->fss) {
            FSEventStreamStop(watcher->fss);
            FSEventStreamInvalidate(watcher->fss);
        }
        CFRunLoopSourceInvalidate(watcher->source);
        CFRunLoopStop(watcher->loop);
        return;
    } else if (watcher->flags & Clear) {
        watcher->flags &= ~Clear;

        if (watcher->fss) {
            FSEventStreamStop(watcher->fss);
            FSEventStreamInvalidate(watcher->fss);
            watcher->fss = 0;
        }

        // We might have paths added since the clear operation was inititated
        if (watcher->paths.empty())
            return;
    }

    // ### might make sense to have multiple streams instead of recreating one for each change
    // ### and then merge them if the stream count reaches a given treshold

    const int pathSize = watcher->paths.size();
    FSEventStreamRef newfss = 0;

    if (pathSize) {
        List<CFStringRef> refs(pathSize);
        int i = 0;
        Set<Path>::const_iterator path = watcher->paths.begin();
        const Set<Path>::const_iterator end = watcher->paths.end();
        while (path != end) {
            refs[i++] = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault,
                                                        path->constData(),
                                                        kCFStringEncodingUTF8,
                                                        kCFAllocatorNull);
            ++path;
        }

        // don't need to hold the mutex beyond this point
        locker.unlock();

        CFArrayRef list = CFArrayCreate(kCFAllocatorDefault,
                                        reinterpret_cast<const void**>(&refs[0]),
                                        pathSize,
                                        &kCFTypeArrayCallBacks);

        for (int j = 0; j < i; ++j)
            CFRelease(refs[j]);

        FSEventStreamContext ctx = { 0, watcher, 0, 0, 0 };
        newfss = FSEventStreamCreate(kCFAllocatorDefault,
                                     notifyCallback,
                                     &ctx,
                                     list,
                                     watcher->since,
                                     .1,
                                     kFSEventStreamCreateFlagIgnoreSelf
                                     | kFSEventStreamCreateFlagFileEvents
                                     );

        CFRelease(list);
    }

    if (!newfss)
        return;

    if (watcher->fss) {
        FSEventStreamStop(watcher->fss);
        FSEventStreamInvalidate(watcher->fss);
    }

    watcher->fss = newfss;

    FSEventStreamScheduleWithRunLoop(watcher->fss, watcher->loop, kCFRunLoopDefaultMode);
    FSEventStreamStart(watcher->fss);
}

void WatcherData::notifyCallback(ConstFSEventStreamRef streamRef,
                                 void *clientCallBackInfo,
                                 size_t numEvents,
                                 void *eventPaths,
                                 const FSEventStreamEventFlags eventFlags[],
                                 const FSEventStreamEventId eventIds[])
{
    WatcherData* watcher = static_cast<WatcherData*>(clientCallBackInfo);
    std::lock_guard<std::mutex> locker(watcher->mutex);
    watcher->since = FSEventStreamGetLatestEventId(streamRef);
    char** paths = reinterpret_cast<char**>(eventPaths);

    Set<Path> created, removed, modified;

    for (size_t i = 0; i < numEvents; ++i) {
        const FSEventStreamEventFlags flags = eventFlags[i];
        if (flags & kFSEventStreamEventFlagHistoryDone)
            continue;
        if (flags & kFSEventStreamEventFlagItemIsFile) {
            if (flags & kFSEventStreamEventFlagItemCreated) {
                created.insert(Path(paths[i]));
            }
            if (flags & kFSEventStreamEventFlagItemRemoved) {
                removed.insert(Path(paths[i]));
            }
            if (flags & (kFSEventStreamEventFlagItemModified | kFSEventStreamEventFlagItemInodeMetaMod)) {
                modified.insert(Path(paths[i]));
            }
        }
    }

    if (!created.empty())
        EventLoop::eventLoop()->callLaterMove(std::bind(&FileSystemWatcher::pathsAdded, watcher->watcher, std::placeholders::_1), std::move(created));
    if (!removed.empty())
        EventLoop::eventLoop()->callLater(std::bind(&FileSystemWatcher::pathsRemoved, watcher->watcher, std::placeholders::_1), std::move(removed));
    if (!modified.empty())
        EventLoop::eventLoop()->callLater(std::bind(&FileSystemWatcher::pathsModified, watcher->watcher, std::placeholders::_1), std::move(modified));
}

FileSystemWatcher::FileSystemWatcher()
    : mWatcher(new WatcherData(this))
{
    mWatcher->waitForStarted();
}

FileSystemWatcher::~FileSystemWatcher()
{
    delete mWatcher;
}

void FileSystemWatcher::clear()
{
    mWatcher->clear();
}

Set<Path> FileSystemWatcher::watchedPaths() const
{
    return mWatcher->watchedPaths();
}

bool FileSystemWatcher::isWatching(const Path& p) const
{
    if (!p.endsWith('/'))
        return isWatching(p + '/');

    std::lock_guard<std::mutex> lock(mWatcher->mutex);
    return mWatcher->paths.contains(p);
}

bool FileSystemWatcher::watch(const Path &p)
{
    Path path = p;
    assert(!path.isEmpty());
    const Path::Type type = path.type();
    switch (type) {
    case Path::File:
        path = path.parentDir();
        // fall through
    case Path::Directory:
        break;
    default:
        error("FileSystemWatcher::watch() '%s' doesn't seem to be watchable", path.constData());
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

void FileSystemWatcher::pathsAdded(const Set<Path>& paths)
{
    for (const Path& path : paths) {
        mAdded(path);
    }
}

void FileSystemWatcher::pathsRemoved(const Set<Path>& paths)
{
    for (const Path& path : paths) {
        mRemoved(path);
    }
}

void FileSystemWatcher::pathsModified(const Set<Path>& paths)
{
    for (const Path& path : paths) {
        mModified(path);
    }
}
