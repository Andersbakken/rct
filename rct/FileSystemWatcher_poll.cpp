#include "rct/FileSystemWatcher.h"
#include "rct/Thread.h"
#include "rct/WaitCondition.h"
#include <sys/types.h>
#include <dirent.h>

static inline uint64_t lastModified(const Path &path)
{
    struct stat statBuf;
    if (!stat(path.constData(), &statBuf)) {
#ifdef HAVE_STATMTIM
        return statBuf.st_mtim.tv_sec * static_cast<uint64_t>(1000) + statBuf.st_mtim.tv_nsec / static_cast<uint64_t>(1000000);
#else
        return st.st_mtime * static_cast<uint64_t>(1000);
#endif
    }
    return 0;
}

class PollThread : public Thread
{
public:
    PollThread(FileSystemWatcher *w)
        : mWatcher(w), mDone(false)
    {}

    void stop();
    virtual void run();

    FileSystemWatcher *mWatcher;
    WaitCondition mCondition;

    Mutex mEventMutex;
    Map<Path, bool> mEvents;
    bool mDone;

    Mutex mFilesMutex;
    struct Directory {
        Map<String, uint64_t> files;
        uint64_t lastModified;
    };
    Map<Path, Directory> mFiles;

};

FileSystemWatcher::FileSystemWatcher()
    : mThread(new PollThread(this))
{
}

FileSystemWatcher::~FileSystemWatcher()
{
    mThread->stop();
    mThread->join();
    delete mThread;
}

void FileSystemWatcher::clear()
{
    MutexLocker lock(&mThread->mMutex);
    mThread->mFiles.clear();
}

bool FileSystemWatcher::watch(const Path &p)
{
    if (p.isFile()) {
        return watch(p.parentDir());
    } else if (!p.isDir()) {
        return false;
    }

    Path path = p;
    if (!path.endsWith('/'))
        path += '/';
    MutexLocker lock(&mThread->mEventMutex);
    mThread->mEvents[p] = true;
}

bool FileSystemWatcher::unwatch(const Path &path)
{
    MutexLocker lock(&mThread->mEventMutex);
    mThread->mEvents[path] = false;
    return true;
}

Set<Path> FileSystemWatcher::watchedPaths() const
{
    MutexLocker lock(&mThread->mFilesMutex);
    return mThread->mFiles.keys().toSet();
}

void PollThread::stop()
{
    MutexLocker lock(&mEventMutex);
    mDone = true;
    mCondition.wakeOne();
}

void PollThread::run()
{
    Path last;
    while (true) {
        Set<Path, bool> events;
        {
            MutexLocker lock(&mEventMutex);
            mCondition.wait(&mEventMutex, 50);
            if (mDone)
                break;
            std::swap(events, mEvents);
        }
        {
            MutexLocker lock(mFilesMutex);
            for (Set<Path, bool>::const_iterator it = events.begin(); it != events.end(); ++it) {
                
            }

            Map<Path, Directory>
        }
    }


    if (mThread->mFiles.contains(path))
        return false;
    DIR *d = opendir(path.constData());
    if (!d)
        return false;
    PollThread::Directory &dir = mThread->mFiles[path];
    dir.lastModified = lastModified(path);

    char buf[PATH_MAX + sizeof(dirent) + 1];
    dirent *dbuf = reinterpret_cast<dirent*>(buf);

    dirent *res;
    const int s = path.size();
    path.reserve(s + 128);
    while (!readdir_r(d, dbuf, &res) && res) {
        if (!strcmp(res->d_name, ".") || !strcmp(res->d_name, ".."))
            continue;
        path.truncate(s);
        String fn = res->d_name;
        path.append(fn);
        const uint64_t mod = lastModified(p.constData());
        if (mod)
            dir.files[fn] = mod;
    }

    closedir(d);
    return true;
}
    
}
