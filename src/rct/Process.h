#ifndef PROCESS_H
#define PROCESS_H

#include <rct/String.h>
#include <rct/EventReceiver.h>
#include <rct/Path.h>
#include <rct/List.h>
#include <rct/SignalSlot.h>
#include <rct/WaitCondition.h>
#include <rct/MutexLocker.h>
#include <rct/Mutex.h>
#include <deque>

class Process : public EventReceiver
{
public:
    Process();
    ~Process();

    void setCwd(const Path& cwd);

    bool start(const String& command, const List<String>& arguments = List<String>());
    bool start(const String& command, const List<String>& arguments,
               const List<String>& environ);

    String errorString() const { MutexLocker lock(&mMutex); return mErrorString; }

    void write(const String& data);
    void closeStdIn();

    String readAllStdOut();
    String readAllStdErr();

    bool isFinished() const { MutexLocker lock(&mMutex); return mPid == -1; }
    int returnCode() const { MutexLocker lock(&mMutex); return mReturn; }

    void stop();

    signalslot::Signal0& readyReadStdOut() { return mReadyReadStdOut; }
    signalslot::Signal0& readyReadStdErr() { return mReadyReadStdErr; }
    signalslot::Signal0& finished() { return mFinished; }

    static List<String> environment();

    static Path findCommand(const String& command);

    bool waitForFinished(int ms = 0);
private:
    void finish(int returnCode);
    static void processCallback(int fd, unsigned int flags, void* userData);

    void closeStdOut();
    void closeStdErr();

    void handleInput(int fd);
    void handleOutput(int fd, String& buffer, int& index, signalslot::Signal0& signal);

private:
    int mStdIn[2];
    int mStdOut[2];
    int mStdErr[2];

    mutable Mutex mMutex;
    WaitCondition mCondition;
    pid_t mPid;
    int mReturn;

    std::deque<String> mStdInBuffer;
    String mStdOutBuffer, mStdErrBuffer;
    int mStdInIndex, mStdOutIndex, mStdErrIndex;

    Path mCwd;

    String mErrorString;

    signalslot::Signal0 mReadyReadStdOut, mReadyReadStdErr, mFinished;

    friend class ProcessThread;
};

#endif
