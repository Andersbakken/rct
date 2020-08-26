#ifdef _WIN32
#error "This file can not be built on Windows. Build Process_Windows.cpp instead"
#else

#include "Process.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#ifdef OS_Darwin
#include <crt_externs.h>
#endif
#include <map>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "EventLoop.h"
#include "Log.h"
#include "SocketClient.h"
#include "Thread.h"
#include "rct/rct-config.h"
#include "rct/Path.h"
#include "rct/Rct.h"

static std::once_flag sProcessHandler;

class ProcessThread : public Thread
{
public:
    static void installProcessHandler();
    static void addPid(pid_t pid, Process *process, bool async);

    /// Remove a pid that was previously added with addPid().
    static void removePid(pid_t pid);
    static void shutdown();
    static void setPending(int pending);

protected:
    virtual void run() override;

private:
    ProcessThread();
    enum Signal { Child, Stop };

    /**
     * Wakeup the listening thread, either because we are to be destructed from
     * the static destructor, or because a child has terminated (sending
     * SIGCHLD).
     */
    static void wakeup(Signal sig);

    static void processSignalHandler(int sig);

private:
    static ProcessThread *sProcessThread;
    static int sProcessPipe[2];

    static std::mutex sProcessMutex;
    static int sPending;
    static std::unordered_map<pid_t, int> sPendingPids;

    struct ProcessData {
        Process *proc;
        std::weak_ptr<EventLoop> loop;
    };
    static std::map<pid_t, ProcessData> sProcesses;
};

ProcessThread *ProcessThread::sProcessThread = nullptr;
int ProcessThread::sProcessPipe[2];
std::mutex ProcessThread::sProcessMutex;
int ProcessThread::sPending = 0;
std::unordered_map<pid_t, int> ProcessThread::sPendingPids;
std::map<pid_t, ProcessThread::ProcessData> ProcessThread::sProcesses;

class ProcessThreadKiller
{
public:
    ~ProcessThreadKiller()
    {
        ProcessThread::shutdown();
    }
} sKiller;

ProcessThread::ProcessThread()
{
    ::signal(SIGCHLD, ProcessThread::processSignalHandler);

    int flg;
    eintrwrap(flg, ::pipe(sProcessPipe));
    SocketClient::setFlags(sProcessPipe[1], O_NONBLOCK, F_GETFL, F_SETFL);
}

void ProcessThread::setPending(int pending)
{
    std::lock_guard<std::mutex> lock(sProcessMutex);
    sPending += pending;
    assert(sPending >= 0);
    if (!pending)
        sPendingPids.clear();
}

void ProcessThread::addPid(pid_t pid, Process *process, bool async)
{
    std::lock_guard<std::mutex> lock(sProcessMutex);
    sPending -= 1;

    if (!sPendingPids.empty()) {
        // we have to check
        auto it = sPendingPids.find(pid);
        if (it != sPendingPids.end()) {
            // fire the signal now
            const int ret = it->second;
            if (async) {
                EventLoop::eventLoop()->callLater([process, ret]() { process->finish(ret); });
            } else {
                process->finish(ret);
            }
            sPendingPids.erase(it);
            if (!sPending)
                sPendingPids.clear();
            return;
        }
    }

    sProcesses[pid] = { process, async ? EventLoop::eventLoop() : std::shared_ptr<EventLoop>() };

    if (!sPending)
        sPendingPids.clear();
}

void ProcessThread::removePid(pid_t f_pid)
{
    std::lock_guard<std::mutex> lock(sProcessMutex);

    auto it = sProcesses.find(f_pid);
    if (it != sProcesses.end()) {
        it->second.proc = nullptr;
        it->second.loop = std::shared_ptr<EventLoop>();
    }
}

void ProcessThread::run()
{
    ssize_t r;
    for (;;) {
        char ch;
        r = ::read(sProcessPipe[0], &ch, sizeof(char));
        if (r == 1) {
            if (ch == 's') {
                break;
            } else {
                int ret;
                pid_t p;
                std::unique_lock<std::mutex> lock(sProcessMutex);
                bool done = false;
                do {
                    // find out which process we're waking up on
                    eintrwrap(p, ::waitpid(0, &ret, WNOHANG));
                    switch (p) {
                    case 0:
                        // we're done
                        done = true;
                        break;
                    case -1:
                        done = true;
                        if (errno == ECHILD)
                            break;
                        // this is bad
                        error() << "waitpid error" << errno;
                        break;
                    default:
                        // printf("successfully waited for pid (got %d)\n", p);
                        // child process with pid=p just exited.
                        if (WIFEXITED(ret)) {
                            ret = WEXITSTATUS(ret);
                        } else {
                            ret = Process::ReturnCrashed;
                        }
                        // find the process object to this child process
                        auto proc = sProcesses.find(p);
                        if (proc != sProcesses.end()) {
                            Process *process = proc->second.proc;
                            if (process != nullptr) {
                                std::shared_ptr<EventLoop> loop = proc->second.loop.lock();
                                sProcesses.erase(proc++);
                                lock.unlock();
                                if (loop) {
                                    loop->callLater([process, ret]() { process->finish(ret); });
                                } else {
                                    process->finish(ret);
                                }
                                lock.lock();
                            }
                        } else {
                            if (sPending) {
                                assert(sPendingPids.find(p) == sPendingPids.end());
                                sPendingPids[p] = ret;
                            } else {
                                error() << "couldn't find process for pid" << p;
                            }
                        }
                    }
                } while (!done);
            }
        }
    }
    // printf("process thread died for some reason\n");
}

void ProcessThread::shutdown()
{
    // called from static destructor, can't use mutex
    if (sProcessThread) {
        wakeup(Stop);
        sProcessThread->join();
        delete sProcessThread;
        sProcessThread = nullptr;
    }
}

void ProcessThread::wakeup(Signal sig)
{
    const char b = sig == Stop ? 's' : 'c';
    // printf("sending pid %d\n", pid);
    do {
        if (::write(sProcessPipe[1], &b, sizeof(char)) >= 0)
            break;
    } while (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK);
    // printf("sent pid %ld\n", written);
}

void ProcessThread::processSignalHandler(int sig)
{
    assert(sig == SIGCHLD);
    (void)sig;
    wakeup(Child);
}

void ProcessThread::installProcessHandler()
{
    assert(sProcessThread == nullptr);
    sProcessThread = new ProcessThread;
    sProcessThread->start();
}

Process::Process()
    : mMode(Sync), mReturn(ReturnUnset),
      mStdInIndex(0), mStdOutIndex(0), mStdErrIndex(0),
      mWantStdInClosed(false), mPid(-1)
{
    std::call_once(sProcessHandler, ProcessThread::installProcessHandler);

    mStdIn[0] = mStdIn[1] = -1;
    mStdOut[0] = mStdOut[1] = -1;
    mStdErr[0] = mStdErr[1] = -1;
    mSync[0] = mSync[1] = -1;
}

Process::~Process()
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        assert(mReturn != ReturnUnset || mPid == -1);
    }

    if (mStdIn[0] != -1 && EventLoop::eventLoop()) {
        // try to finish off any pending writes
        handleInput(mStdIn[1]);
    }

    closeStdIn(CloseForce);
    closeStdOut();
    closeStdErr();

    int w;
    if (mSync[0] != -1)
        eintrwrap(w, ::close(mSync[0]));
    if (mSync[1] != -1)
        eintrwrap(w, ::close(mSync[1]));
}

void Process::clear()
{
    // don't clear a running process please
    assert(mPid == -1);

    if (mStdIn[0] != -1 && EventLoop::eventLoop()) {
        // try to finish off any pending writes
        handleInput(mStdIn[1]);
    }

    closeStdIn(CloseForce);
    closeStdOut();
    closeStdErr();

    int w;
    if (mSync[0] != -1)
        eintrwrap(w, ::close(mSync[0]));
    if (mSync[1] != -1)
        eintrwrap(w, ::close(mSync[1]));

    mReturn = ReturnUnset;
    mStdInIndex = mStdOutIndex = mStdErrIndex = 0;
    mWantStdInClosed = false;
    mMode = Sync;
}

void Process::setCwd(const Path &cwd)
{
    assert(mReturn == ReturnUnset);
    mCwd = cwd;
}

Path Process::findCommand(const String &command, const char *path)
{
    /// @todo use Path::isAbsolute() and check if the file actually exists
    if (command.isEmpty() || command.at(0) == '/')
        return command;

    if (!path)
        path = getenv("PATH");
    if (!path)
        return Path();
    bool ok;
    const List<String> paths = String(path).split(':');
    for (List<String>::const_iterator it = paths.begin(); it != paths.end(); ++it) {
        const Path ret = Path::resolved(command, Path::RealPath, *it, &ok);
        if (ok && !access(ret.nullTerminated(), R_OK | X_OK))
            return ret;
    }
    return Path();
}

Process::ExecState Process::startInternal(const Path &command, const List<String> &a,
                                          const List<String> &environment, int timeout,
                                          unsigned int execFlags)
{
    mErrorString.clear();

    const char *path = nullptr;
    for (const auto &it : environment) {
        if (it.startsWith("PATH=")) {
            path = it.constData() + 5;
            break;
        }
    }
    Path cmd = findCommand(command, path);
    if (cmd.isEmpty()) {
        mErrorString = "Command not found";
        return Error;
    }
    List<String> arguments = a;
    int err;

    int closePipe[2];
    eintrwrap(err, ::pipe(closePipe));
#ifdef HAVE_CLOEXEC
    if (!SocketClient::setFlags(closePipe[1], FD_CLOEXEC, F_GETFD, F_SETFD)) {
        mErrorString = "Unable to set FD_CLOEXEC";
        eintrwrap(err, ::close(closePipe[0]));
        eintrwrap(err, ::close(closePipe[1]));
        return Error;
    }
#else
#warning No CLOEXEC, Process might have problematic behavior
#endif

    eintrwrap(err, ::pipe(mStdIn));
    eintrwrap(err, ::pipe(mStdOut));
    eintrwrap(err, ::pipe(mStdErr));
    if (mMode == Sync)
        eintrwrap(err, ::pipe(mSync));

    const char **args = new const char *[arguments.size() + 2];
    // const char* args[arguments.size() + 2];
    args[arguments.size() + 1] = nullptr;
    args[0] = cmd.nullTerminated();
    int pos = 1;
    for (List<String>::const_iterator it = arguments.begin(); it != arguments.end(); ++it) {
        args[pos] = it->nullTerminated();
        // printf("arg: '%s'\n", args[pos]);
        ++pos;
    }

    const bool hasEnviron = !environment.empty();

    const char **env = new const char *[environment.size() + 1];
    env[environment.size()] = nullptr;

    if (hasEnviron) {
        pos = 0;
        // printf("fork, about to exec '%s'\n", cmd.nullTerminated());
        for (List<String>::const_iterator it = environment.begin(); it != environment.end(); ++it) {
            env[pos] = it->nullTerminated();
            // printf("env: '%s'\n", env[pos]);
            ++pos;
        }
    }

    ProcessThread::setPending(1);

    pid_t oldPid;
    oldPid = mPid = ::fork();
    if (mPid == -1) {
        // printf("fork, something horrible has happened %d\n", errno);
        // bail out

        ProcessThread::setPending(-1);

        eintrwrap(err, ::close(mStdIn[1]));
        eintrwrap(err, ::close(mStdIn[0]));
        eintrwrap(err, ::close(mStdOut[1]));
        eintrwrap(err, ::close(mStdOut[0]));
        eintrwrap(err, ::close(mStdErr[1]));
        eintrwrap(err, ::close(mStdErr[0]));
        eintrwrap(err, ::close(closePipe[1]));
        eintrwrap(err, ::close(closePipe[0]));
        mErrorString = "Fork failed";
        delete[] env;
        delete[] args;
        return Error;
    } else if (mPid == 0) {
        // printf("fork, in child\n");
        // child, should do some error checking here really
        eintrwrap(err, ::close(closePipe[0]));
        eintrwrap(err, ::close(mStdIn[1]));
        eintrwrap(err, ::close(mStdOut[0]));
        eintrwrap(err, ::close(mStdErr[0]));

        eintrwrap(err, ::close(STDIN_FILENO));
        eintrwrap(err, ::close(STDOUT_FILENO));
        eintrwrap(err, ::close(STDERR_FILENO));

        eintrwrap(err, ::dup2(mStdIn[0], STDIN_FILENO));
        eintrwrap(err, ::close(mStdIn[0]));
        eintrwrap(err, ::dup2(mStdOut[1], STDOUT_FILENO));
        eintrwrap(err, ::close(mStdOut[1]));
        eintrwrap(err, ::dup2(mStdErr[1], STDERR_FILENO));
        eintrwrap(err, ::close(mStdErr[1]));

        int ret;
        if (!mChRoot.isEmpty() && ::chroot(mChRoot.constData())) {
            goto error;
        }
        if (!mCwd.isEmpty() && ::chdir(mCwd.constData())) {
            goto error;
        }
        if (hasEnviron) {
            ret = ::execve(cmd.nullTerminated(), const_cast<char *const *>(args), const_cast<char *const *>(env));
        } else {
            ret = ::execv(cmd.nullTerminated(), const_cast<char *const *>(args));
        }
        // notify the parent process
  error:
        const char c = 'c';
        eintrwrap(err, ::write(closePipe[1], &c, 1));
        eintrwrap(err, ::close(closePipe[1]));
        ::_exit(1);
        (void)ret;
        // printf("fork, exec seemingly failed %d, %d %s\n", ret, errno, Rct::strerror().constData());
    } else {
        delete[] env;
        delete[] args;

        // parent
        eintrwrap(err, ::close(closePipe[1]));
        eintrwrap(err, ::close(mStdIn[0]));
        eintrwrap(err, ::close(mStdOut[1]));
        eintrwrap(err, ::close(mStdErr[1]));

        // printf("fork, in parent\n");

        int flags;
        eintrwrap(flags, fcntl(mStdIn[1], F_GETFL, 0));
        eintrwrap(flags, fcntl(mStdIn[1], F_SETFL, flags | O_NONBLOCK));
        eintrwrap(flags, fcntl(mStdOut[0], F_GETFL, 0));
        eintrwrap(flags, fcntl(mStdOut[0], F_SETFL, flags | O_NONBLOCK));
        eintrwrap(flags, fcntl(mStdErr[0], F_GETFL, 0));
        eintrwrap(flags, fcntl(mStdErr[0], F_SETFL, flags | O_NONBLOCK));

        // block until exec is called in the child or until exec fails
        {
            char c;
            eintrwrap(err, ::read(closePipe[0], &c, 1));
            (void)c;

            if (err == -1) {
                // bad
                eintrwrap(err, ::close(closePipe[0]));
                mErrorString = "Failed to read from closePipe during process start";
                mPid = -1;
                ProcessThread::setPending(-1);
                mReturn = ReturnCrashed;
                return Error;
            } else if (err == 0) {
                // process has started successfully
                eintrwrap(err, ::close(closePipe[0]));
            } else if (err == 1) {
                // process start failed
                eintrwrap(err, ::close(closePipe[0]));
                mErrorString = "Process failed to start";
                mReturn = ReturnCrashed;
                mPid = -1;
                ProcessThread::setPending(-1);
                return Error;
            }
        }

        ProcessThread::addPid(mPid, this, (mMode == Async));

        // printf("fork, about to add fds: stdin=%d, stdout=%d, stderr=%d\n", mStdIn[1], mStdOut[0], mStdErr[0]);
        if (mMode == Async) {
            if (std::shared_ptr<EventLoop> loop = EventLoop::eventLoop()) {
                loop->registerSocket(mStdOut[0], EventLoop::SocketRead,
                                     std::bind(&Process::processCallback, this, std::placeholders::_1, std::placeholders::_2));
                loop->registerSocket(mStdErr[0], EventLoop::SocketRead,
                                     std::bind(&Process::processCallback, this, std::placeholders::_1, std::placeholders::_2));
            }
        } else {
            // select and stuff
            timeval started, now, timeoutForSelect;
            if (timeout > 0) {
                Rct::gettime(&started);
                timeoutForSelect.tv_sec = timeout / 1000;
                timeoutForSelect.tv_usec = (timeout % 1000) * 1000;
            }
            if (!(execFlags & NoCloseStdIn)) {
                closeStdIn(CloseForce);
                mWantStdInClosed = false;
            }
            for (;;) {
                // set up all the select crap
                fd_set rfds, wfds;
                FD_ZERO(&rfds);
                FD_ZERO(&wfds);
                int max = 0;
                FD_SET(mStdOut[0], &rfds);
                max = std::max(max, mStdOut[0]);
                FD_SET(mStdErr[0], &rfds);
                max = std::max(max, mStdErr[0]);
                FD_SET(mSync[0], &rfds);
                max = std::max(max, mSync[0]);
                if (mStdIn[1] != -1) {
                    FD_SET(mStdIn[1], &wfds);
                    max = std::max(max, mStdIn[1]);
                }
                int ret;
                eintrwrap(ret, ::select(max + 1, &rfds, &wfds, nullptr, timeout > 0 ? &timeoutForSelect : nullptr));
                if (ret == -1) { // ow
                    mErrorString = "Sync select failed: ";
                    mErrorString += Rct::strerror();
                    return Error;
                }
                // check fds and stuff
                if (FD_ISSET(mStdOut[0], &rfds))
                    handleOutput(mStdOut[0], mStdOutBuffer, mStdOutIndex, mReadyReadStdOut);
                if (FD_ISSET(mStdErr[0], &rfds))
                    handleOutput(mStdErr[0], mStdErrBuffer, mStdErrIndex, mReadyReadStdErr);
                if (mStdIn[1] != -1 && FD_ISSET(mStdIn[1], &wfds))
                    handleInput(mStdIn[1]);
                if (FD_ISSET(mSync[0], &rfds)) {
                    // we're done
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        assert(mSync[1] == -1);

                        // try to read all remaining data on stdout and stderr
                        handleOutput(mStdOut[0], mStdOutBuffer, mStdOutIndex, mReadyReadStdOut);
                        handleOutput(mStdErr[0], mStdErrBuffer, mStdErrIndex, mReadyReadStdErr);

                        closeStdOut();
                        closeStdErr();

                        int w;
                        eintrwrap(w, ::close(mSync[0]));
                        mSync[0] = -1;
                    }
                    mFinished(this, oldPid);
                    return Done;
                }
                if (timeout) {
                    Rct::gettime(&now);

                    // lasted is the amount of time we spent until now in ms
                    const int lasted = Rct::timevalDiff(&now, &started);
                    if (lasted >= timeout) {
                        // timeout, we're done
                        kill(); // attempt to kill

                        // we need to remove this Process object from
                        // ProcessThread, because ProcessThread will try to
                        // finish() this object. However, this object may
                        // already have been deleted *before* ProcessThread
                        // runs, creating a segfault.
                        ProcessThread::removePid(mPid);

                        mErrorString = "Timed out";
                        return TimedOut;
                    }

                    // (timeout - lasted) is guaranteed to be > 0 because of
                    // the check above.
                    timeoutForSelect.tv_sec = (timeout - lasted) / 1000;
                    timeoutForSelect.tv_usec = ((timeout - lasted) % 1000) * 1000;
                }
            }
        }
    }
    return Done;
}

bool Process::start(const Path &command, const List<String> &a, const List<String> &environment)
{
    mMode = Async;
    return startInternal(command, a, environment) == Done;
}

Process::ExecState Process::exec(const Path &command, const List<String> &arguments, int timeout, unsigned int flags)
{
    mMode = Sync;
    return startInternal(command, arguments, List<String>(), timeout, flags);
}

Process::ExecState Process::exec(
    const Path &command, const List<String> &a, const List<String> &environment, int timeout, unsigned int flags)
{
    mMode = Sync;
    return startInternal(command, a, environment, timeout, flags);
}

void Process::write(const String &data)
{
    if (!data.isEmpty() && mStdIn[1] != -1) {
        mStdInBuffer.push_back(data);
        handleInput(mStdIn[1]);
    }
}

void Process::closeStdIn(CloseStdInFlag flag)
{
    if (mStdIn[1] == -1)
        return;

    if (flag == CloseForce || mStdInBuffer.empty()) {
        if (std::shared_ptr<EventLoop> loop = EventLoop::eventLoop())
            loop->unregisterSocket(mStdIn[1]);
        int err;
        eintrwrap(err, ::close(mStdIn[1]));
        mStdIn[1] = -1;
        mWantStdInClosed = false;
    } else {
        mWantStdInClosed = true;
    }
}

void Process::closeStdOut()
{
    if (mStdOut[0] == -1)
        return;

    if (std::shared_ptr<EventLoop> eventLoop = EventLoop::eventLoop())
        eventLoop->unregisterSocket(mStdOut[0]);
    int err;
    eintrwrap(err, ::close(mStdOut[0]));
    mStdOut[0] = -1;
}

void Process::closeStdErr()
{
    if (mStdErr[0] == -1)
        return;

    if (std::shared_ptr<EventLoop> eventLoop = EventLoop::eventLoop())
        eventLoop->unregisterSocket(mStdErr[0]);
    int err;
    eintrwrap(err, ::close(mStdErr[0]));
    mStdErr[0] = -1;
}

String Process::readAllStdOut()
{
    String out;
    std::swap(mStdOutBuffer, out);
    mStdOutIndex = 0;
    return out;
}

String Process::readAllStdErr()
{
    String out;
    std::swap(mStdErrBuffer, out);
    mStdErrIndex = 0;
    return out;
}

void Process::processCallback(int fd, int mode)
{
    if (mode == EventLoop::SocketError) {
        // we're closed, shut down
        return;
    }
    if (fd == mStdIn[1])
        handleInput(fd);
    else if (fd == mStdOut[0])
        handleOutput(fd, mStdOutBuffer, mStdOutIndex, mReadyReadStdOut);
    else if (fd == mStdErr[0])
        handleOutput(fd, mStdErrBuffer, mStdErrIndex, mReadyReadStdErr);
}

void Process::finish(int returnCode)
{
    pid_t oldPid;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        oldPid = mPid;
        mReturn = returnCode;

        mStdInBuffer.clear();
        closeStdIn(CloseForce);
        mWantStdInClosed = false;

        if (mMode == Async) {
            // try to read all remaining data on stdout and stderr
            handleOutput(mStdOut[0], mStdOutBuffer, mStdOutIndex, mReadyReadStdOut);
            handleOutput(mStdErr[0], mStdErrBuffer, mStdErrIndex, mReadyReadStdErr);

            closeStdOut();
            closeStdErr();
        } else {
            int w;
            char q = 'q';
            eintrwrap(w, ::write(mSync[1], &q, 1));
            eintrwrap(w, ::close(mSync[1]));
            mSync[1] = -1;
        }

        mPid = -1;
    }

    if (mMode == Async)
        mFinished(this, oldPid);
}

void Process::handleInput(int fd)
{
    assert(EventLoop::eventLoop());
    EventLoop::eventLoop()->unregisterSocket(fd);

    // static int ting = 0;
    // printf("Process::handleInput (cnt=%d)\n", ++ting);
    for (;;) {
        if (mStdInBuffer.empty())
            return;

        // printf("Process::handleInput in loop\n");
        int w, want;
        const String &front = mStdInBuffer.front();
        if (mStdInIndex) {
            want = front.size() - mStdInIndex;
            eintrwrap(w, ::write(fd, front.mid(mStdInIndex).constData(), want));
        } else {
            want = front.size();
            eintrwrap(w, ::write(fd, front.constData(), want));
        }
        if (w == -1) {
            EventLoop::eventLoop()->registerSocket(
                fd, EventLoop::SocketWrite, std::bind(&Process::processCallback, this, std::placeholders::_1, std::placeholders::_2));
            break;
        } else if (w == want) {
            mStdInBuffer.pop_front();
            if (mStdInBuffer.empty() && mWantStdInClosed) {
                EventLoop::eventLoop()->unregisterSocket(mStdIn[1]);
                int err;
                eintrwrap(err, ::close(mStdIn[1]));
                mStdIn[1] = -1;
                mWantStdInClosed = false;
            }
            mStdInIndex = 0;
        } else
            mStdInIndex += w;
    }
}

void Process::handleOutput(int fd, String &buffer, int &index, Signal<std::function<void(Process *)>> &signal)
{
    // printf("Process::handleOutput %d\n", fd);
    enum { BufSize = 1024, MaxSize = (1024 * 1024 * 256) };
    char buf[BufSize];
    int total = 0;
    for (;;) {
        int r;
        eintrwrap(r, ::read(fd, buf, BufSize));
        if (r == -1) {
            // printf("Process::handleOutput %d returning -1, errno %d %s\n", fd, errno, Rct::strerror().constData());
            break;
        } else if (r == 0) { // file descriptor closed, remove it
            // printf("Process::handleOutput %d returning 0\n", fd);
            if (auto eventLoop = EventLoop::eventLoop())
                eventLoop->unregisterSocket(fd);
            break;
        } else {
            // printf("Process::handleOutput in loop %d\n", fd);
            // printf("data: '%s'\n", String(buf, r).constData());
            int sz = buffer.size();
            if (sz + r > MaxSize) {
                if (sz + r - index > MaxSize) {
                    error("Process::handleOutput, buffer too big, dropping data");
                    buffer.clear();
                    index = sz = 0;
                } else {
                    sz = buffer.size() - index;
                    memmove(buffer.data(), buffer.data() + index, sz);
                    buffer.resize(sz);
                    index = 0;
                }
            }
            buffer.resize(sz + r);
            memcpy(buffer.data() + sz, buf, r);

            total += r;
        }
    }

    // printf("total data '%s'\n", buffer.nullTerminated());

    if (total)
        signal(this);
}

void Process::kill(int sig)
{
    if (mReturn != ReturnUnset || mPid == -1)
        return;

    mReturn = ReturnKilled;
    ::kill(mPid, sig);
}

List<String> Process::environment()
{
#ifdef OS_Darwin
    char **cur = *_NSGetEnviron();
#else
    extern char **environ;
    char **cur = environ;
#endif
    List<String> env;
    while (*cur) {
        env.push_back(*cur);
        ++cur;
    }
    return env;
}

void Process::setChRoot(const Path &path)
{
    assert(mReturn == ReturnUnset);
    mChRoot = path;
}

int Process::returnCode() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mReturn;
}

bool Process::isFinished() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mReturn != ReturnUnset;
}

String Process::errorString() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mErrorString;
}

pid_t Process::pid() const
{
    return mPid;
}

#endif // not _WIN32
