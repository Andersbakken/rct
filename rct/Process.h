#ifndef PROCESS_H
#define PROCESS_H

#include <signal.h>
#include <rct/List.h>
#include <rct/Path.h>
#include <rct/SignalSlot.h>
#include <rct/String.h>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "rct/List.h"
#include "rct/SignalSlot.h"
#include "rct/String.h"

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
     *
     * @param flags see ExecFlag. Ignored on windows.
     */
    ExecState exec(const Path &command, const List<String> &arguments = List<String>(),
                   int timeout_ms = 0, unsigned int flags = 0);

    /**
     * Execute a child process synchronously, specifying an environment.
     *
     * You can get a default environment by calling environment().
     *
     * @param flags see ExecFlag. Ignored on windows.
     * @param environ Each String in the List must be in the form of key=value.
     *                On windows, these strings must be encoded in utf8.
     * @see Process::environment()
     */
    ExecState exec(const Path &command, const List<String> &arguments,
                   const List<String> &f_environ, int timeout_ms = 0,
                   unsigned int flags = 0);

    /**
     * Get information about errors that occured while trying to execute the
     * child process.
     * Note: This is *not* the child process' stderr.
     */
    String errorString() const;

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
     * The read data is removed from internal storage, so that another call
     * call to readAllStdOut() will not return the same data again.
     * Does not block.
     */
    String readAllStdOut();

    /**
     * Read all data that the child process has written to stderr so far.
     * The read data is removed from internal storage, so that another call
     * call to readAllStdErr() will not return the same data again.
     * Does not block.
     */
    String readAllStdErr();

    bool isFinished() const;

    enum { ReturnCrashed = -1, ReturnUnset = -2, ReturnKilled = -3 };
    int returnCode() const;

    /**
     * Send the requested signal to the process.
     *
     * Windows: Apparently, sending signals is not supported on windows
     * (although one CAN set signal handlers...), so the windows implementation
     * kills the process if signal is SIGTERM. For other values, this
     * method call is ignored.
     */
    void kill(int signal = SIGTERM);

public:
    Signal<std::function<void(Process*)> > &readyReadStdOut() { return mReadyReadStdOut; }
    Signal<std::function<void(Process*)> > &readyReadStdErr() { return mReadyReadStdErr; }
    Signal<std::function<void(Process *, pid_t)> > &finished() { return mFinished; }

    /**
     * Get this process' environment. This can be used as a starting point to
     * build the child process' environment.
     *
     * On windows, this list will be encoded as utf-8.
     *
     * @return A list of strings in the format <key>=<value>.
     */
    static List<String> environment();

    /**
     * Find a command in the current PATH.
     * If nothing can be found, an empty Path is returned.
     */
    static Path findCommand(const String &command, const char *path = nullptr);

    /**
     * Get the child process' process id.
     */
    pid_t pid() const;

    /**
     * Clear a child process' information and close all data structure *after*
     * a child process terminated, so that this Process object can be used to
     * start another child process.
     * You don't need to call this before the destructor.
     */
    void clear();

private:
    /**
     * Start a process asynchronously.
     */
    ExecState startInternal(const Path &command, const List<String> &arguments,
                            const List<String> &f_environ, int timeout = 0,
                            unsigned int flags = 0);

private:  // members for windows and non-windows implementations

    /// Will be notified when child sends something over its stdout
    Signal<std::function<void(Process*)> > mReadyReadStdOut;

    /// Will be notified when child sends something over its stderr
    Signal<std::function<void(Process*)> > mReadyReadStdErr;

    /// Will be notified when the child process finishes.
    Signal<std::function<void(Process *, pid_t)> > mFinished;

    enum { Sync, Async } mMode;

    std::deque<String> mStdInBuffer;

    String mStdOutBuffer;  ///< Collects data from child's stdout. @see readAllStdOut()

    String mStdErrBuffer;  ///< Collects data from child's stderr. @see readAllStdErr()

    mutable std::mutex mMutex;

    /// Child process' return value or one of the Return* values above
    int mReturn;

    Path mCwd, mChRoot;

    String mErrorString;

#ifdef _WIN32
private:  // members only required for the windows implementation

    /**
     * Closes the handle if it is != INVALID_HANDLE_VALUE and sets the handle to INVALID_HANDLE_VALUE.
     */
    static void closeHandleIfValid(HANDLE &hdl);

    enum PipeToReadFrom {STDOUT, STDERR};
    void readFromPipe(PipeToReadFrom pipe);

    void waitForProcessToFinish();

    void manageTimeout(int timeout_ms);
private:
    enum
    {
        READ_END,
        WRITE_END,
        NUM_HANDLES
    };

    static const int PIPE_READ_BUFFER_SIZE = 512;

    HANDLE mStdIn[NUM_HANDLES];
    HANDLE mStdOut[NUM_HANDLES];
    HANDLE mStdErr[NUM_HANDLES];
    PROCESS_INFORMATION mProcess;  ///< don't forget to close handles!

    std::thread mthStdout, mthStderr, mthManageTimeout;
    enum {NO_TIMEOUT, FINISHED_ON_ITS_OWN, KILLED, NOT_FINISHED} mProcessTimeoutStatus;
    std::condition_variable mProcessFinished_cond;

    std::mutex mStdinMutex;
#else  // _WIN32
private:  // member functions not required for the windows implementation
    void finish(int returnCode);
    void processCallback(int fd, int mode);

    void closeStdOut();
    void closeStdErr();

    void handleInput(int fd);
    void handleOutput(int fd, String &buffer, int &index, Signal<std::function<void(Process*)> > &signal);

private:  // members not required for the windows implementation

    int mStdIn[2];   ///< Pipe used by the child as stdin
    int mStdOut[2];  ///< Pipe used by the child as stdout
    int mStdErr[2];  ///< Pipe used by the child as stderr
    int mSync[2];    ///< used to quit waiting for the child in sync mode

    int mStdInIndex, mStdOutIndex, mStdErrIndex;
    bool mWantStdInClosed;
    pid_t mPid;   ///< Child process' pid

    friend class ProcessThread;
#endif  // _WIN32
};

#endif
