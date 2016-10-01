#ifndef PROCESS_H
#define PROCESS_H

#include <signal.h>
#include <deque>
#include <mutex>

#include <rct/List.h>
#include <rct/Path.h>
#include <rct/SignalSlot.h>
#include <rct/String.h>

/**
 * This class can be used to launch a child process, monitor its execution
 * (including stdout, stderr) and send data through stdin.
 */
class Process
{
public:
    /**
     * Constructor.
     */
    Process();

    /**
     * Destructor. Only valid if the child process has terminated (or was never
     * started).
     */
    ~Process();

    /**
     * Set the child's working directory.
     */
    void setCwd(const Path &cwd);

    /**
     * Set the child's root directory (/). Not supported on Windows.
     */
    void setChRoot(const Path &path);

    /**
     * Run a child process asynchronously.
     * Processing stdout/stderr from the child process is performed through an
     * EventLoop, so there needs to be a running event loop for it to work.
     */
    bool start(const Path &command,
               const List<String> &arguments = List<String>(),
               const List<String> &f_environ = List<String>());

    enum ExecState { Error, Done, TimedOut };

    enum ExecFlag {
        None = 0x0,
        NoCloseStdIn = 0x1
    };

    /**
     * Execute a child process synchronously
     */
    ExecState exec(const Path &command, const List<String> &arguments = List<String>(),
                   int timeout = 0, unsigned int flags = 0);

    /**
     * Execute a child process synchronously, specifying an environment.
     * @param environ Each String in the List must be in the form of key=value.
     */
    ExecState exec(const Path &command, const List<String> &arguments,
                   const List<String> &f_environ, int timeout = 0, unsigned int flags = 0);

    /**
     * Get information about errors that occured while trying to execute the
     * child process.
     * Note: This is *not* the child process' stderr.
     */
    String errorString() const { std::lock_guard<std::mutex> lock(mMutex); return mErrorString; }

    /**
     * Write data to the child process' stdin.
     */
    void write(const String &data);

    enum CloseStdInFlag
    {
        CloseNormal,  ///< close stdin once all data has been read
        CloseForce    ///< force to close stdin, even if data is pending
    };

    /**
     * Close the child process' stdin pipe.
     */
    void closeStdIn(CloseStdInFlag flag = CloseNormal);

    /**
     * Read all data that the child process has written to stdout so far.
     * Does not block.
     */
    String readAllStdOut();

    /**
     * Read all data that the child process has written to stderr so far.
     * Does not block.
     */
    String readAllStdErr();

    bool isFinished() const { std::lock_guard<std::mutex> lock(mMutex); return mReturn != ReturnUnset; }
    int returnCode() const { std::lock_guard<std::mutex> lock(mMutex); return mReturn; }

    void kill(int signal = SIGTERM);

    Signal<std::function<void(Process*)> > &readyReadStdOut() { return mReadyReadStdOut; }
    Signal<std::function<void(Process*)> > &readyReadStdErr() { return mReadyReadStdErr; }
    Signal<std::function<void(Process*)> > &finished() { return mFinished; }

    /**
     * Get this process' environment. This can be used as a starting point to
     * build the child process' environment.
     */
    static List<String> environment();

    /**
     * Find a command in the current PATH.
     * If nothing can be found, an empty Path is returned.
     */
    static Path findCommand(const String &command, const char *path = 0);

    /**
     * Get the child process' process id.
     */
    pid_t pid() const { return mPid; }

    /**
     * Clear a child process' information and close all data structure *after*
     * a child process terminated, so that this Process object can be used to
     * start another child process.
     * You don't need to call this before the destructor.
     */
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

    int mStdIn[2];   ///< Pipe used by the child as stdin
    int mStdOut[2];  ///< Pipe used by the child as stdout
    int mStdErr[2];  ///< Pipe used by the child as stderr
    int mSync[2];    ///< used to quit waiting for the child in sync mode

    mutable std::mutex mMutex;
    pid_t mPid;   ///< Child process' pid
    enum { ReturnCrashed = -1, ReturnUnset = -2, ReturnKilled = -3 };

    /// Child process' return value or one of the Return* values above
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
