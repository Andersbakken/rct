#include "FileSystemWatcher.h"

FileSystemWatcher::FileSystemWatcher(const Options &options)
    : mOptions(options)
{
    init();
    mTimer.timeout().connect([this](Timer *) { processChanges(Add|Remove); });
}

void FileSystemWatcher::processChanges()
{
    if (mOptions.removeDelay > 0) {
        processChanges(Modified);
        if (!mRemovedPaths.empty())
            mTimer.restart(mOptions.removeDelay);
    } else {
        processChanges(Modified|Add|Remove);
    }
}

void FileSystemWatcher::processChanges(unsigned int types)
{
    std::lock_guard<std::mutex> lock(mMutex);
    assert(types);
    struct {
        const Type type;
        Signal<std::function<void(const Path&)> > &signal;
        Set<Path> &paths;
    } signals[] = {
        { Add, mAdded, mAddedPaths },
        { Remove, mRemoved, mRemovedPaths },
        { Modified, mModified, mModifiedPaths }
    };

    const unsigned int count = sizeof(signals) / sizeof(signals[0]);
    for (unsigned i=0; i<count; ++i) {
        if (types & signals[i].type) {
            for (Set<Path>::const_iterator it = signals[i].paths.begin(); it != signals[i].paths.end(); ++it) {
                signals[i].signal(*it);
            }
            signals[i].paths.clear();
        }
    }
}

