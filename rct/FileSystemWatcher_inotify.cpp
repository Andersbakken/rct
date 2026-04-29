#include <assert.h>
#include <errno.h>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <utility>

#include "EventLoop.h"
#include "FileSystemWatcher.h"
#include "Log.h"
#include "Rct.h"
#include "StackBuffer.h"
#include "rct/Map.h"
#include "rct/Path.h"
#include "rct/Set.h"
#include "rct/String.h"
#include "rct/rct-config.h"

void FileSystemWatcher::init()
{
    mFd = inotify_init();
    assert(mFd != -1);
    int res     = fcntl(mFd, F_SETFD, FD_CLOEXEC);
    std::ignore = res;
    assert(res != -1);
    EventLoop::eventLoop()->registerSocket(mFd, EventLoop::SocketRead, std::bind(&FileSystemWatcher::notifyReadyRead, this));
}

void FileSystemWatcher::shutdown()
{
    EventLoop::eventLoop()->unregisterSocket(mFd);
    for (Map<Path, int>::const_iterator it = mWatchedByPath.begin(); it != mWatchedByPath.end(); ++it) {
        inotify_rm_watch(mFd, it->second);
    }
    close(mFd);
}

void FileSystemWatcher::clear()
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (Map<Path, int>::const_iterator it = mWatchedByPath.begin(); it != mWatchedByPath.end(); ++it) {
        inotify_rm_watch(mFd, it->second);
    }
    mWatchedByPath.clear();
    mWatchedById.clear();
}

bool FileSystemWatcher::watch(const Path &p)
{
    if (p.empty())
        return false;
    Path path = p;
    assert(!path.empty());
    std::lock_guard<std::mutex> lock(mMutex);
    const Path::Type type = path.type();
    uint32_t flags        = 0;
    switch (type) {
        case Path::File:
            // IN_MODIFY catches in-place writers; IN_CLOSE_WRITE catches
            // editors that close after writing; IN_DELETE_SELF/IN_MOVE_SELF
            // detect rename-replace (cmake-style atomic writes), after which
            // we re-arm in notifyReadyRead.
            flags = IN_DELETE_SELF | IN_MOVE_SELF | IN_ATTRIB | IN_DELETE | IN_CLOSE_WRITE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO;
            break;
        case Path::Directory:
            flags = IN_MOVED_FROM | IN_MOVED_TO | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_ATTRIB | IN_CLOSE_WRITE;
            if (!path.endsWith('/'))
                path.append('/');
            break;
        default:
            error("FileSystemWatcher::watch() '%s' doesn't seem to be watchable", path.c_str());
            return false;
    }

    if (mWatchedByPath.contains(path)) {
        return false;
    }
    const int ret = inotify_add_watch(mFd, path.c_str(), flags);
    if (ret == -1) {
        error("FileSystemWatcher::watch() watch failed for '%s' (%d) %s", path.c_str(), errno, Rct::strerror().c_str());
        return false;
    }

    mWatchedByPath[path] = ret;
    mWatchedById[ret]    = path;
    return true;
}

bool FileSystemWatcher::unwatch(const Path &path)
{
    std::lock_guard<std::mutex> lock(mMutex);
    int wd = -1;
    if (mWatchedByPath.remove(path, &wd)) {
        debug("FileSystemWatcher::unwatch(\"%s\")", path.c_str());
        mWatchedById.remove(wd);
        inotify_rm_watch(mFd, wd);
        return true;
    } else {
        return false;
    }
}

static inline void dump(Log &log, unsigned int mask)
{
    if (mask & IN_ACCESS) {
        mask &= ~IN_ACCESS;
        log << "IN_ACCESS";
    }
    if (mask & IN_MODIFY) {
        mask &= ~IN_MODIFY;
        log << "IN_MODIFY";
    }
    if (mask & IN_ATTRIB) {
        mask &= ~IN_ATTRIB;
        log << "IN_ATTRIB";
    }
    if (mask & IN_CLOSE_WRITE) {
        mask &= ~IN_CLOSE_WRITE;
        log << "IN_CLOSE_WRITE";
    }
    if (mask & IN_CLOSE_NOWRITE) {
        mask &= ~IN_CLOSE_NOWRITE;
        log << "IN_CLOSE_NOWRITE";
    }
    if (mask & IN_CLOSE) {
        mask &= ~IN_CLOSE;
        log << "IN_CLOSE";
    }
    if (mask & IN_OPEN) {
        mask &= ~IN_OPEN;
        log << "IN_OPEN";
    }
    if (mask & IN_MOVED_FROM) {
        mask &= ~IN_MOVED_FROM;
        log << "IN_MOVED_FROM";
    }
    if (mask & IN_MOVED_TO) {
        mask &= ~IN_MOVED_TO;
        log << "IN_MOVED_TO";
    }
    if (mask & IN_CREATE) {
        mask &= ~IN_CREATE;
        log << "IN_CREATE";
    }
    if (mask & IN_DELETE) {
        mask &= ~IN_DELETE;
        log << "IN_DELETE";
    }
    if (mask & IN_DELETE_SELF) {
        mask &= ~IN_DELETE_SELF;
        log << "IN_DELETE_SELF";
    }
    if (mask & IN_MOVE_SELF) {
        mask &= ~IN_MOVE_SELF;
        log << "IN_MOVE_SELF";
    }

    if (mask & IN_UNMOUNT) {
        mask &= ~IN_UNMOUNT;
        log << "IN_UNMOUNT";
    }

    if (mask & IN_Q_OVERFLOW) {
        mask &= ~IN_Q_OVERFLOW;
        log << "IN_Q_OVERFLOW";
    }

    if (mask & IN_IGNORED) {
        mask &= ~IN_IGNORED;
        log << "IN_IGNORED";
    }

    assert(!mask);
}

void FileSystemWatcher::notifyReadyRead()
{
    static const bool dumpFS = getenv("RTAGS_DUMP_INOTIFY") && !strcmp(getenv("RTAGS_DUMP_INOTIFY"), "1");
    if (dumpFS) {
        error() << "FileSystemWatcher::notifyReadyRead";
    }
    Set<Path> rearm;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        int s = 0;
        ioctl(mFd, FIONREAD, &s);
        if (!s)
            return;

        StackBuffer<4096> buf(s);
        const int read = ::read(mFd, buf, s);
        int idx        = 0;
        while (idx < read) {
            inotify_event *event = reinterpret_cast<inotify_event *>(buf + idx);
            idx += sizeof(inotify_event) + event->len;
            Path path = mWatchedById.value(event->wd);
            if (path.empty())
                continue;

            if (dumpFS && event->mask) {
                Log log(LogLevel::Error);
                log << (path + event->name);
                dump(log, event->mask);
            }

            // IN_IGNORED is the kernel telling us the watch descriptor is
            // gone (file was deleted/replaced, or we removed the watch).
            // Drop our bookkeeping; if the path still exists we re-add below.
            if (event->mask & IN_IGNORED) {
                mWatchedById.remove(event->wd);
                mWatchedByPath.remove(path);
                if (path.exists())
                    rearm.insert(path);
                continue;
            }

            const bool isDir = path.isDir();

            if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT)) {
                add(Remove, path);
                // The kernel will follow up with IN_IGNORED. The path may
                // already have been replaced (atomic rename); queue a re-arm
                // so we don't go deaf to subsequent updates.
                if (!(event->mask & IN_UNMOUNT))
                    rearm.insert(path);
            } else if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                if (isDir)
                    path.append(event->name);
                add(Add, path);
            } else if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
                if (isDir)
                    path.append(event->name);
                add(Remove, path);
            } else if (event->mask & (IN_ATTRIB | IN_CLOSE_WRITE | IN_MODIFY)) {
                if (isDir)
                    path.append(event->name);
                add(Modified, path);
            }
        }
        if (dumpFS) {
            if (!mAddedPaths.empty())
                error() << "Added" << mAddedPaths;
            if (!mRemovedPaths.empty())
                error() << "Removed" << mRemovedPaths;
            if (!mModifiedPaths.empty())
                error() << "Modified" << mModifiedPaths;
        }
    }

    for (const Path &p : rearm) {
        if (!p.exists())
            continue;
        // watch() takes mMutex itself; call it outside the lock above.
        // It is a no-op if the path is already (re-)registered.
        if (!watch(p) && dumpFS) {
            error() << "FileSystemWatcher: re-arm of" << p << "did not register a new watch";
        } else if (dumpFS) {
            error() << "FileSystemWatcher: re-armed" << p;
        }
        // Synthesize a Modified event: clients that special-case this path
        // (e.g. compile_commands.json reload) need to know the contents may
        // have changed during the rename window.
        {
            std::lock_guard<std::mutex> lock(mMutex);
            add(Modified, p);
        }
    }

    processChanges();
}
