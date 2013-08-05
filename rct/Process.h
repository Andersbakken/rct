#ifndef PROCESS_H
#define PROCESS_H

#include <rct/String.h>
#include <rct/Path.h>
#include <rct/List.h>
#include <rct/SignalSlot.h>
#include <deque>
#include <mutex>

class Process
{
public:
    Process();
    ~Process();

    void setCwd(const Path& cwd);

    bool start(const String& command,
               const List<String>& arguments = List<String>(),
               const List<String>& environ = List<String>());

    enum ExecState { Error, Done, TimedOut };

    enum ExecFlag {
        None = 0x0,
        NoCloseStdIn = 0x1
    };
    ExecState exec(const String& command, const List<String>& arguments = List<String>(),
                   int timeout = 0, unsigned flags = 0);
    ExecState exec(const String& command, const List<String>& arguments,
                   const List<String>& environ, int timeout = 0, unsigned flags = 0);

    String errorString() const { std::lock_guard<std::mutex> lock(mMutex); return mErrorString; }

    void write(const String& data);
    void closeStdIn();

    String readAllStdOut();
    String readAllStdErr();

    bool isFinished() const { std::lock_guard<std::mutex> lock(mMutex); return mPid == -1; }
    int returnCode() const { std::lock_guard<std::mutex> lock(mMutex); return mReturn; }

    void stop();

    Signal<std::function<void(Process*)> >& readyReadStdOut() { return mReadyReadStdOut; }
    Signal<std::function<void(Process*)> >& readyReadStdErr() { return mReadyReadStdErr; }
    Signal<std::function<void(Process*)> >& finished() { return mFinished; }

    static List<String> environment();

    static Path findCommand(const String& command);

private:
    void finish(int returnCode);
    void processCallback(int fd, int mode);

    void closeStdOut();
    void closeStdErr();

    void handleInput(int fd);
    void handleOutput(int fd, String& buffer, int& index, Signal<std::function<void(Process*)> >& signal);

    ExecState startInternal(const String& command, const List<String>& arguments,
                            const List<String>& environ, int timeout = 0, unsigned flags = 0);

private:

    int mStdIn[2];
    int mStdOut[2];
    int mStdErr[2];
    int mSync[2];

    mutable std::mutex mMutex;
    pid_t mPid;
    int mReturn;

    std::deque<String> mStdInBuffer;
    String mStdOutBuffer, mStdErrBuffer;
    int mStdInIndex, mStdOutIndex, mStdErrIndex;

    Path mCwd;

    String mErrorString;

    enum { Sync, Async } mMode;

    Signal<std::function<void(Process*)> > mReadyReadStdOut, mReadyReadStdErr, mFinished;

    friend class ProcessThread;
};

#endif
