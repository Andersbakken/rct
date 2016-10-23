#include "FileSystemWatcher.h"

#include <assert.h>
#include <windows.h>
#ifdef HAVE_CYGWIN
#include <sys/cygwin.h>
#endif
#include <thread>


class WatcherSlice
{
public:
    WatcherSlice(std::vector<HANDLE>::iterator& begin,
                 std::vector<HANDLE>::iterator& end,
                 std::function<bool(const Path&)>&& notif,
                 const std::map<HANDLE, Path>& paths);
    ~WatcherSlice();

    void run();

    std::vector<HANDLE> changes;
    bool stopped;
    std::function<bool(const Path&)> notify;
    std::map<HANDLE, Path> handleToPath;
    std::thread thread;
    std::mutex mutex;
};

WatcherSlice::WatcherSlice(std::vector<HANDLE>::iterator& begin,
                           std::vector<HANDLE>::iterator& end,
                           std::function<bool(const Path&)>&& notif,
                           const std::map<HANDLE, Path>& paths)
    : changes(begin, end), stopped(false),
      notify(std::forward<std::function<bool(const Path&)> >(notif)),
      handleToPath(paths)
{
    const HANDLE h = CreateEvent(NULL, FALSE, FALSE, NULL);
    changes.push_back(h);

    thread = std::thread(std::bind(&WatcherSlice::run, this));
}

WatcherSlice::~WatcherSlice()
{
    assert(!changes.empty());
    {
        std::lock_guard<std::mutex> locker(mutex);
        stopped = true;
        SetEvent(changes.back());
    }
    thread.join();
    CloseHandle(changes.back());
}

void WatcherSlice::run()
{
    assert(changes.size() <= MAXIMUM_WAIT_OBJECTS);
    for (;;) {
        const DWORD ret = WaitForMultipleObjects(changes.size(), &changes[0], FALSE, INFINITE);
        if (ret == WAIT_FAILED) {
            fprintf(stderr, "Wait failed in WatcherSlice::run() %lu\n", static_cast<unsigned long>(GetLastError()));
            break;
        }
        const unsigned int idx = ret - WAIT_OBJECT_0;
        if (idx == changes.size() - 1) {
            // woken up, probably due to stop
            std::lock_guard<std::mutex> locker(mutex);
            if (stopped)
                break;
        }
        //printf("woken up due to idx %d\n", idx);
        assert(handleToPath.count(changes[idx]));
        if (!notify(handleToPath[changes[idx]]))
            break;
    }
}

class WatcherData : public std::enable_shared_from_this<WatcherData>
{
public:
    WatcherData(FileSystemWatcher* w)
        : stopped(false), changed(true), watcher(w)
    {
    }

    void updatePaths();
    void wakeup();
    void stop();
    void run();
    bool hasChanged();
    bool updated(const Path& path);

    Set<Path> paths;
    Set<Path> changedPaths;
    std::vector<std::unique_ptr<WatcherSlice> > slices;

    struct PathData
    {
        PathData() { }

        Set<Path> added, changed, seen;
        Map<Path, uint64_t> modified;
    };
    std::map<Path, PathData> pathData;

    bool stopped;
    bool changed;
    HANDLE wakeupHandle;
    std::vector<HANDLE> changes;
    std::map<HANDLE, Path> handleToPath;
    std::map<Path, HANDLE> pathToHandle;
    std::thread thread;
    std::mutex changeMutex, updateMutex;
    FileSystemWatcher* watcher;
};

void WatcherData::updatePaths()
{
    // printf("updating paths...\n");
    {
        std::lock_guard<std::mutex> updateLocker(updateMutex);
        handleToPath.clear();
        pathToHandle.clear();
        pathData.clear();
    }
    for (HANDLE& h : changes) {
        //printf("closing %d\n", h);
        FindCloseChangeNotification(h);
    }
    changes.clear();

    std::lock_guard<std::mutex> locker(changeMutex);
    for(const Path& path : paths) {
#ifdef HAVE_CYGWIN
        const ssize_t len = cygwin_conv_path(CCP_POSIX_TO_WIN_A | CCP_ABSOLUTE, path.constData(), 0, 0);
        //printf("win path size %d\n", len);
        String winPath(len, '\0');
        cygwin_conv_path(CCP_POSIX_TO_WIN_A | CCP_ABSOLUTE, path.constData(), winPath.data(), winPath.size());
        //printf("hello %s\n", winPath.constData());
        const HANDLE h = FindFirstChangeNotification(winPath.constData(), TRUE,
                                                     FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
#else
        const HANDLE h = FindFirstChangeNotification(path.constData(), TRUE,
                                                     FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
#endif
        if (h == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "Unable to watch: %lu (%s)\n",
                    static_cast<unsigned long>(GetLastError()), path.constData());
        } else {
            changes.push_back(h);

            std::lock_guard<std::mutex> updateLocker(updateMutex);
            handleToPath[h] = path;
            pathToHandle[path] = h;
            PathData& data = pathData[path];
            path.visit([&data](const Path &p) {
                    if (p.isFile()) {
                        data.modified[p] = p.lastModifiedMs();
                        return Path::Continue;
                    }
                    return Path::Recurse;
                });
        }
    }
}

bool WatcherData::updated(const Path& path)
{
    //printf("updated %s\n", path.constData());
    std::lock_guard<std::mutex> locker(updateMutex);
    const auto h = pathToHandle.find(path);
    if (h == pathToHandle.end()) {
        //printf("handle not found in pathToHandle\n");
        return false;
    }
    FindNextChangeNotification(h->second);

    // notify the main thread
    changedPaths.insert(path);
    SetEvent(wakeupHandle);
    return true;
}

bool WatcherData::hasChanged()
{
    std::lock_guard<std::mutex> locker(changeMutex);
    if (changed) {
        changed = false;
        return true;
    }
    return false;
}

void WatcherData::run()
{
    for (;;) {
        if (hasChanged()) {
            slices.clear();
            updatePaths();

            auto where = changes.begin();
            const auto end = changes.end();
            while (where != end) {
                auto to = where + MAXIMUM_WAIT_OBJECTS - 1; // - 1 since we want a wakeup event as well
                if (to > end)
                    to = end;
                slices.push_back(std::unique_ptr<WatcherSlice>(new WatcherSlice(where, to,
                                                                                std::bind(&WatcherData::updated, this,
                                                                                          std::placeholders::_1),
                                                                                handleToPath)));
                where = to;
            }
        }
        const DWORD res = WaitForSingleObject(wakeupHandle, INFINITE);
        if (res == WAIT_FAILED) {
            fprintf(stderr, "Wait failed in WatcherData::run() %lu\n",
                    static_cast<unsigned long>(GetLastError()));
            break;
        }
        assert(res - WAIT_OBJECT_0 == 0);
        // woken up
        //printf("!!!Woken up\n");
        std::lock_guard<std::mutex> changeLocker(changeMutex);
        if (stopped) {
            //printf("!!!! Stopped?\n");
            break;
        }
        std::lock_guard<std::mutex> updateLocker(updateMutex);
        if (!changedPaths.empty()) {
            for (const Path& p : changedPaths) {
                //printf("path was modified... %s\n", p.constData());
                PathData& data = pathData[p];
                p.visit([&data](const Path &pp) {
                        if (pp.isFile()) {
                            //printf("updateDir %s\n", p.constData());
                            const auto modif = data.modified.find(pp);
                            if (modif == data.modified.end()) {
                                //printf("added\n");
                                // new file
                                data.added.insert(pp);
                                return Path::Continue;
                            }
                            data.seen.insert(pp);
                            // possibly modified file
                            if (pp.lastModifiedMs() != modif->second) {
                                //printf("modified\n");
                                // really modified
                                data.changed.insert(pp);
                            }
                            return Path::Continue;
                        }
                        return Path::Recurse;
                    });

                Set<Path> removed;
                // calculate the removed files (modified - seen)
                const auto send = data.seen.end();
                for (const std::pair<Path, uint64_t>& mod : data.modified) {
                    if (data.seen.find(mod.first) == send) {
                        removed.insert(mod.first);
                    } else {
                        // update to our new time
                        data.modified[mod.first] = mod.first.lastModifiedMs();
                    }
                }

                // update the modified structure
                for (const Path& ap : data.added) {
                    data.modified[ap] = ap.lastModifiedMs();
                }
                for (const Path& rp : removed) {
                    data.modified.erase(rp);
                }

                //printf("hei, removed %u, added %u, changed %u\n", removed.size(), data.added.size(), data.changed.size());
                if (!removed.empty())
                    EventLoop::mainEventLoop()->callLaterMove(std::bind(&FileSystemWatcher::pathsRemoved, watcher, std::placeholders::_1), std::move(removed));
                if (!data.added.empty())
                    EventLoop::mainEventLoop()->callLaterMove(std::bind(&FileSystemWatcher::pathsAdded, watcher, std::placeholders::_1), std::move(data.added));
                if (!data.changed.empty())
                    EventLoop::mainEventLoop()->callLaterMove(std::bind(&FileSystemWatcher::pathsModified, watcher, std::placeholders::_1), std::move(data.changed));

                data.added.clear();
                data.changed.clear();
                data.seen.clear();
            }
            changedPaths.clear();
        }
    }
    slices.clear();
}

// needs to have mutex locked
void WatcherData::wakeup()
{
    //printf("wanting to wake up\n");
    SetEvent(wakeupHandle);
}

void WatcherData::stop()
{
    {
        std::lock_guard<std::mutex> locker(changeMutex);
        stopped = true;
        wakeup();
    }
    thread.join();

    for (HANDLE& h : changes) {
        FindCloseChangeNotification(h);
    }
    changes.clear();
}

void FileSystemWatcher::init()
{
    mWatcher.reset(new WatcherData(this));
    mWatcher->wakeupHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
    mWatcher->thread = std::thread(std::bind(&WatcherData::run, mWatcher));
}

void FileSystemWatcher::shutdown()
{
    mWatcher->stop();
    CloseHandle(mWatcher->wakeupHandle);
    mWatcher.reset();
}

void FileSystemWatcher::clear()
{
    std::lock_guard<std::mutex> locker(mWatcher->changeMutex);
    //printf("clearing watched\n");
    mWatcher->paths.clear();
    mWatcher->changed = true;
    mWatcher->wakeup();
}

Set<Path> FileSystemWatcher::watchedPaths() const
{
    std::lock_guard<std::mutex> locker(mWatcher->changeMutex);
    return mWatcher->paths;
}

bool FileSystemWatcher::watch(const Path& p)
{
    std::lock_guard<std::mutex> locker(mWatcher->changeMutex);
    //printf("watching %s\n", p.constData());
    mWatcher->paths.insert(p);
    mWatcher->changed = true;
    mWatcher->wakeup();
    return true;
}

bool FileSystemWatcher::unwatch(const Path& p)
{
    std::lock_guard<std::mutex> locker(mWatcher->changeMutex);
    //printf("unwatching %s\n", p.constData());
    mWatcher->paths.erase(p);
    mWatcher->changed = true;
    mWatcher->wakeup();
    return true;
}

void FileSystemWatcher::pathsAdded(const Set<Path>& paths)
{
    for (const Path& path : paths) {
        //printf("really added %s\n", path.constData());
        mAdded(path);
    }
}

void FileSystemWatcher::pathsRemoved(const Set<Path>& paths)
{
    for (const Path& path : paths) {
        //printf("really removed %s\n", path.constData());
        mRemoved(path);
    }
}

void FileSystemWatcher::pathsModified(const Set<Path>& paths)
{
    for (const Path& path : paths) {
        //printf("really modified %s\n", path.constData());
        mModified(path);
    }
}
