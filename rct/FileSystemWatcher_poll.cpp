#include "rct/FileSystemWatcher.h"
#include "rct/Thread.h"
#include "rct/MutexLocker.h"
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
        return statBuf.st_mtime * static_cast<uint64_t>(1000);
#endif
    }
    return 0;
}

class PollThread : public Thread
{
public:
    PollThread(FileSystemWatcher *w)
        : mWatcher(w), mClear(false), mDone(false)
    {}

    void stop();
    virtual void run();

    void prepareDirectory(const Path &path);

    FileSystemWatcher *mWatcher;
    WaitCondition mCondition;

    Mutex mEventMutex;
    Map<Path, bool> mEvents;
    bool mClear;
    bool mDone;

    Mutex mFilesMutex;
    struct Directory {
        Map<String, uint64_t> files;
        uint64_t lastModified;
    };
    Map<Path, Directory> mWatched;

};

FileSystemWatcher::FileSystemWatcher()
    : mThread(new PollThread(this))
{
    mThread->start();
}

FileSystemWatcher::~FileSystemWatcher()
{
    mThread->stop();
    mThread->join();
    delete mThread;
}

void FileSystemWatcher::clear()
{
    MutexLocker lock(&mThread->mEventMutex);
    mThread->mClear = true;
    mThread->mCondition.wakeOne();
}

bool FileSystemWatcher::watch(const Path &p)
{
    error() << "Calling watch" << p;
    if (p.isFile()) {
        return watch(p.parentDir());
    } else if (p.isDir() && !p.endsWith('/')) {
        return watch(p + '/');
    }
    {
        MutexLocker lock(&mThread->mFilesMutex);
        if (mThread->mWatched.contains(p))
            return false;
    }

    MutexLocker lock(&mThread->mEventMutex);
    mThread->mEvents[p] = true;
    mThread->mCondition.wakeOne();
    printf("[%s:%d]: mThread->mEvents[p] = true;\n", __func__, __LINE__); fflush(stdout);
    return true;
}

bool FileSystemWatcher::unwatch(const Path &path)
{
    MutexLocker lock(&mThread->mEventMutex);
    if (path.isDir() && !path.endsWith('/')) {
        mThread->mEvents[path + '/'] = false;
    } else {
        mThread->mEvents[path] = false;
    }
    mThread->mCondition.wakeOne();
    return true;
}

Set<Path> FileSystemWatcher::watchedPaths() const
{
    MutexLocker lock(&mThread->mFilesMutex);
    return mThread->mWatched.keysAsSet();
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
    bool empty = true;
    Path lastModifiedPath;
    bool ordered = true;
    while (true) {
        Map<Path, bool> events;
        bool clear = false;
        {
            MutexLocker lock(&mEventMutex);
            while (mEvents.empty() && !mDone && !mClear) {
                // error() << "waiting" << empty;
                // error() << "about to sleep" << mEvents << mDone << mClear;
                // printf("[%s:%d]: sleep(1);\n", __func__, __LINE__); fflush(stdout);
                // sleep(1);
                // {
                //     lock.unlock();
                //     lock.relock();
                // }
                // if (!empty) {
                //     printf("[%s:%d]: if (!empty) {\n", __func__, __LINE__); fflush(stdout);
                //     break;
                // }

                if (!mCondition.wait(&mEventMutex, 1000)) { //empty ? 0 : 1000)) {
                    // error() << "woke up from timeout";
                    break;
                }
            }
            if (mDone)
                break;
            std::swap(events, mEvents);
            std::swap(clear, mClear);
        }
        // error() << "woke up" << events.size() << mClear << last;
        {
            MutexLocker lock(&mFilesMutex);
            if (mClear) {
                mClear = false;
                mWatched.clear();
            }
            for (Map<Path, bool>::const_iterator it = events.begin(); it != events.end(); ++it) {
                error() << "processing" << it->first << it->second;
                if (!it->second) {
                    error() << "removing path" << it->first;
                    mWatched.remove(it->first);
                } else if (!mWatched.contains(it->first)) {
                    prepareDirectory(it->first);
                }
            }
        }
        empty = mWatched.isEmpty();
        // error() << "About to scan" << empty << last;
        if (!empty) {
            Path path;
            if (ordered) {
                Map<Path, Directory>::const_iterator it = mWatched.lower_bound(last);
                if (it->first == last)
                    ++it;
                if (it == mWatched.end())
                    it = mWatched.begin();
                last = it->first;
                path = it->first;
                if (!lastModifiedPath.isEmpty())
                    ordered = false;
            } else {
                ordered = true;
                path = lastModifiedPath;
            }
            // error() << "scanning" << last;

            DIR *d = opendir(path.constData());
            if (d) {
                char buf[PATH_MAX + sizeof(dirent) + 1];
                dirent *dbuf = reinterpret_cast<dirent*>(buf);

                dirent *res;
                Directory &dir = mWatched[path];
                uint64_t mod = lastModified(path);
                if (dir.lastModified != mod) {
                    // files removed or added. Not sure if useful
                    dir.lastModified = mod;
                }

                const int s = path.size();
                path.reserve(s + 128);
                Set<String> seen;
                bool found = false;
                while (!readdir_r(d, dbuf, &res) && res) {
                    if (!strcmp(res->d_name, ".") || !strcmp(res->d_name, ".."))
                        continue;
                    path.truncate(s);
                    const String fn = res->d_name;
                    path.append(fn);
                    seen.insert(fn);
                    mod = lastModified(path);
                    uint64_t &cur = dir.files[fn];
                    if (mod != cur) {
                        found = true;
                        if (!cur) {
                            // error() << "Added" << path << cur << mod;
                            mWatcher->mAdded(path);
                        } else {
                            // error() << "Modified" << path << cur << mod;
                            mWatcher->mModified(path);
                        }
                        cur = mod;
                    }
                }
                if (seen.size() != dir.files.size()) { // something removed
                    found = true;
                    // error() << "differences" << seen << dir.files.keys();
                    Map<String, uint64_t>::iterator p = dir.files.begin();
                    while (p != dir.files.end()) {
                        if (!seen.contains(p->first)) {
                            error() << path + p->first << "seems to have been removed";
                            mWatcher->mRemoved(path + p->first);
                            dir.files.erase(p++);
                        } else {
                            ++p;
                        }
                    }
                }
                if (found) {
                    lastModifiedPath = path;
                }
                closedir(d);
            } else {
                error() << "couldn't opendir" << last;
            }
        }
    }
}

    // if (mThread->mFiles.contains(path))
    //     return false;
    // DIR *d = opendir(path.constData());
    // if (!d)
    //     return false;
    // PollThread::Directory &dir = mThread->mFiles[path];
    // dir.lastModified = lastModified(path);

    // char buf[PATH_MAX + sizeof(dirent) + 1];
    // dirent *dbuf = reinterpret_cast<dirent*>(buf);

    // dirent *res;
    // const int s = path.size();
    // path.reserve(s + 128);
    // while (!readdir_r(d, dbuf, &res) && res) {
    //     if (!strcmp(res->d_name, ".") || !strcmp(res->d_name, ".."))
    //         continue;
    //     path.truncate(s);
    //     String fn = res->d_name;
    //     path.append(fn);
    //     const uint64_t mod = lastModified(p.constData());
    //     if (mod)
    //         dir.files[fn] = mod;
    // }

    // closedir(d);
    // return true;
// }
    
void PollThread::prepareDirectory(const Path &p)
{
    char buf[PATH_MAX + sizeof(dirent) + 1];
    dirent *dbuf = reinterpret_cast<dirent*>(buf);

    dirent *res;
    Path path = p;
    DIR *d = opendir(path.constData());
    error() << "adding" << path;
    if (d) {
        Directory &dir = mWatched[path];
        dir.lastModified = lastModified(path);
        const int s = path.size();
        path.reserve(s + 128);
        while (!readdir_r(d, dbuf, &res) && res) {
            error() << "got dude" << res->d_name;
            if (!strcmp(res->d_name, ".") || !strcmp(res->d_name, ".."))
                continue;
            path.truncate(s);
            String fn = res->d_name;
            path.append(fn);
            const uint64_t mod = lastModified(path);
            if (mod)
                dir.files[fn] = mod;
        }
        closedir(d);
        error() << path << "is set up" << dir.files;
    } else {
        error() << "Can't opendir" << path;
    }
}

