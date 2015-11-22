#ifndef PROCESS_H
#define PROCESS_H

#include <signal.h>
#include <deque>
#include <mutex>

#include <rct/List.h>
#include <rct/Path.h>
#include <rct/SignalSlot.h>
#include <rct/String.h>

class Process
{
public:
    Process();
    ~Process();

    void setCwd(const Path &cwd);
    void setChRoot(const Path &path);

    bool start(const Path &command,
               const List<String> &arguments = List<String>(),
               const List<String> &environ = List<String>());

    enum ExecState { Error, Done, TimedOut };

    enum ExecFlag {
        None = 0x0,
        NoCloseStdIn = 0x1
    };
    ExecState exec(const Path &command, const List<String> &arguments = List<String>(),
                   int timeout = 0, unsigned int flags = 0);
    ExecState exec(const Path &command, const List<String> &arguments,
                   const List<String> &environ, int timeout = 0, unsigned int flags = 0);

    String errorString() const { std::lock_guard<std::mutex> lock(mMutex); return mErrorString; }

    void write(const String &data);

    enum CloseStdInFlag { CloseNormal, CloseForce };
    void closeStdIn(CloseStdInFlag flag = CloseNormal);

    String readAllStdOut();
    String readAllStdErr();

    bool isFinished() const { std::lock_guard<std::mutex> lock(mMutex); return mReturn != ReturnUnset; }
    int returnCode() const { std::lock_guard<std::mutex> lock(mMutex); return mReturn; }

    void kill(int signal = SIGTERM);

    Signal<std::function<void(Process*)> > &readyReadStdOut() { return mReadyReadStdOut; }
    Signal<std::function<void(Process*)> > &readyReadStdErr() { return mReadyReadStdErr; }
    Signal<std::function<void(Process*)> > &finished() { return mFinished; }

    static List<String> environment();

    static Path findCommand(const String &command, const char *path = 0);

    pid_t pid() const { return mPid; }

    void clear();

private:
    void finish(int returnCode);
    void processCallback(int fd, int mode);

    void closeStdOut();
    void closeStdErr();

    void handleInput(int fd);
    void handleOutput(int fd, String &buffer, int &index, Signal<std::function<void(Process*)> > &signal);

    ExecState startInternal(const Path &command, const List<String> &arguments,
                            const List<String> &environ, int timeout = 0, unsigned int flags = 0);

private:

    int mStdIn[2];
    int mStdOut[2];
    int mStdErr[2];
    int mSync[2];

    mutable std::mutex mMutex;
    pid_t mPid;
    enum { ReturnCrashed = -1, ReturnUnset = -2, ReturnKilled = -3 };
    int mReturn;

    std::deque<String> mStdInBuffer;
    String mStdOutBuffer, mStdErrBuffer;
    int mStdInIndex, mStdOutIndex, mStdErrIndex;
    bool mWantStdInClosed;

    Path mCwd, mChRoot;

    String mErrorString;

    enum { Sync, Async } mMode;

    Signal<std::function<void(Process*)> > mReadyReadStdOut, mReadyReadStdErr, mFinished;

    friend class ProcessThread;
};

#endif
