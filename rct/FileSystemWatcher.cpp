#include "FileSystemWatcher.h"

#include <assert.h>
#include <map>
#include <set>
#include <utility>

#include "rct/Path.h"
#include "rct/Set.h"
#include "rct/SignalSlot.h"
#include "rct/Timer.h"

FileSystemWatcher::FileSystemWatcher(const Options &options)
    : mOptions(options)
{
    init();
    mTimer.timeout().connect([this](Timer *) {
            processChanges(Remove);
        });
}

FileSystemWatcher::~FileSystemWatcher()
{
    mTimer.stop();
    shutdown();
}

void FileSystemWatcher::processChanges()
{
    if (mOptions.removeDelay > 0) {
        processChanges(Add|Modified);
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mRemovedPaths.empty())
                mTimer.restart(mOptions.removeDelay);
        }
    } else {
        processChanges(Modified|Add|Remove);
    }
}

void FileSystemWatcher::processChanges(unsigned int types)
{
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
            Set<Path> p;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                std::swap(p, signals[i].paths);
            }

            for (Set<Path>::const_iterator it = p.begin(); it != p.end(); ++it) {
                signals[i].signal(*it);
            }
        }
    }
}

