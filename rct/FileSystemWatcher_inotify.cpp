#include "FileSystemWatcher.h"
#include "EventLoop.h"
#include "Log.h"
#include "rct-config.h"
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <errno.h>

FileSystemWatcher::FileSystemWatcher()
{
    mFd = inotify_init();
    assert(mFd != -1);
    EventLoop::eventLoop()->registerSocket(mFd, EventLoop::SocketRead, std::bind(&FileSystemWatcher::notifyReadyRead, this));
}

FileSystemWatcher::~FileSystemWatcher()
{
    EventLoop::eventLoop()->unregisterSocket(mFd);
    for (Map<Path, int>::const_iterator it = mWatchedByPath.begin(); it != mWatchedByPath.end(); ++it) {
        inotify_rm_watch(mFd, it->second);
    }
    close(mFd);
}

void FileSystemWatcher::clear()
{
    for (Map<Path, int>::const_iterator it = mWatchedByPath.begin(); it != mWatchedByPath.end(); ++it) {
        inotify_rm_watch(mFd, it->second);
    }
    mWatchedByPath.clear();
    mWatchedById.clear();
}

bool FileSystemWatcher::watch(const Path &p)
{
    if (p.isEmpty())
        return false;
    Path path = p;
    assert(!path.isEmpty());
    std::lock_guard<std::mutex> lock(mMutex);
    const Path::Type type = path.type();
    uint32_t flags = 0;
    switch (type) {
    case Path::File:
        flags = IN_DELETE_SELF|IN_MOVE_SELF|IN_ATTRIB|IN_DELETE|IN_CLOSE_WRITE;
        break;
    case Path::Directory:
        flags = IN_MOVED_FROM|IN_MOVED_TO|IN_CREATE|IN_DELETE|IN_DELETE_SELF|IN_ATTRIB|IN_CLOSE_WRITE;
        if (!path.endsWith('/'))
            path.append('/');
        break;
    default:
        error("FileSystemWatcher::watch() '%s' doesn't seem to be watchable", path.constData());
        return false;
    }

    if (mWatchedByPath.contains(path)) {
        return false;
    }
    const int ret = inotify_add_watch(mFd, path.nullTerminated(), flags);
    if (ret == -1) {
        error("FileSystemWatcher::watch() watch failed for '%s' (%d) %s",
              path.constData(), errno, strerror(errno));
        return false;
    }

    mWatchedByPath[path] = ret;
    mWatchedById[ret] = path;
    return true;
}

bool FileSystemWatcher::unwatch(const Path &path)
{
    std::lock_guard<std::mutex> lock(mMutex);
    int wd = -1;
    if (mWatchedByPath.remove(path, &wd)) {
        debug("FileSystemWatcher::unwatch(\"%s\")", path.constData());
        mWatchedById.remove(wd);
        inotify_rm_watch(mFd, wd);
        return true;
    } else {
        return false;
    }
}

static inline void dump(unsigned mask)
{
    if (mask & IN_ACCESS)
        printf("IN_ACCESS ");
    if (mask & IN_MODIFY)
        printf("IN_MODIFY ");
    if (mask & IN_ATTRIB)
        printf("IN_ATTRIB ");
    if (mask & IN_CLOSE_WRITE)
        printf("IN_CLOSE_WRITE ");
    if (mask & IN_CLOSE_NOWRITE)
        printf("IN_CLOSE_NOWRITE ");
    if (mask & IN_CLOSE)
        printf("IN_CLOSE ");
    if (mask & IN_OPEN)
        printf("IN_OPEN ");
    if (mask & IN_MOVED_FROM)
        printf("IN_MOVED_FROM ");
    if (mask & IN_MOVED_TO)
        printf("IN_MOVED_TO ");
    if (mask & IN_CREATE)
        printf("IN_CREATE ");
    if (mask & IN_DELETE)
        printf("IN_DELETE ");
    if (mask & IN_DELETE_SELF)
        printf("IN_DELETE_SELF ");
    if (mask & IN_MOVE_SELF)
        printf("IN_MOVE_SELF ");
}

void FileSystemWatcher::notifyReadyRead()
{
    Changes changes;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        int s = 0;
        ioctl(mFd, FIONREAD, &s);
        if (!s)
            return;
        enum { StaticBufSize = 4096 };
        char staticBuf[StaticBufSize];
        char *buf = s > StaticBufSize ? new char[s] : staticBuf;
        const int read = ::read(mFd, buf, s);
        int idx = 0;
        while (idx < read) {
            inotify_event *event = reinterpret_cast<inotify_event*>(buf + idx);
            idx += sizeof(inotify_event) + event->len;
            Path path = mWatchedById.value(event->wd);
            // printf("%s [%s]", path.constData(), event->name);
            // dump(event->mask);
            // printf("\n");

            const bool isDir = path.isDir();

            if (event->mask & (IN_DELETE_SELF|IN_MOVE_SELF|IN_UNMOUNT)) {
                changes.add(Changes::Remove, path);
            } else if (event->mask & (IN_CREATE|IN_MOVED_TO)) {
                changes.add(Changes::Add, path);
            } else if (event->mask & (IN_DELETE|IN_MOVED_FROM)) {
                changes.add(Changes::Remove, path);
            } else if (event->mask & (IN_ATTRIB|IN_CLOSE_WRITE)) {
                if (isDir)
                    path.append(event->name);
                changes.add(Changes::Modified, path);
            }
        }
        if (buf != staticBuf)
            delete []buf;
    }
    processChanges(changes);
}
