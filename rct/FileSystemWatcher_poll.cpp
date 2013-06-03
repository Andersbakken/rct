#include "rct/FileSystemWatcher.h"
#include "rct/Thread.h"

class PollThread : public Thread
{
public:
    PollThread(FileSystemWatcher *w)
        : mWatcher(w)
    {}

    static Path::VisitResult visit(const Path& path, void* userData);
    static Path::VisitResult init(const Path& path, void* userData);

    void stop();
    virtual void run();

    FileSystemWatcher *mWatcher;
    Mutex mMutex;
    WaitCondition mCondition;
    Map<Path, Map<String, uint64_t> > mFiles;
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
    } else if (p.isDir() && !dir.endsWith('/')) {
        return watch(p + '/');
    }
    MutexLocker lock(&mThread->mMutex);
    if (mThread->mFiles.contains(p))
        return false;
    Map<String, uint64_t> &dir = mThread->mFiles[p];
    p.visit(PollThread::init, &dir);
    return true;
}

bool FileSystemWatcher::unwatch(const Path &path)
{
    MutexLocker lock(&mMutex);
    return mFiles.remove(path);
}

Set<Path> FileSystemWatcher::watchedPaths() const
{
    MutexLocker lock(&mMutex);
    return mFiles.keys();
}
