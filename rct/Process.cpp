#include "rct/Process.h"
#include "rct/Rct.h"
#include "rct/EventLoop.h"
#include "rct/Log.h"
#include "rct/Thread.h"
#include "rct/Mutex.h"
#include "rct/MutexLocker.h"
#include "rct-config.h"
#include <map>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef OS_Darwin
#include <crt_externs.h>
#endif

static pthread_once_t sProcessHandler = PTHREAD_ONCE_INIT;

class ProcessThread : public Thread
{
public:
    static void stop();
    static void installProcessHandler();
    static void addPid(pid_t pid, Process* process);
    static void removePid(pid_t pid);

protected:
    void run();

private:
    ProcessThread();

#ifdef HAVE_SIGINFO
    static void processSignalHandler(int sig, siginfo_t* info, void* /*context*/);
#else
    static void processSignalHandler(int sig);
#endif
    static void sendPid(pid_t pid);

private:
    static ProcessThread* sProcessThread;
    static int sProcessPipe[2];

    static Mutex sProcessMutex;
    static std::map<pid_t, Process*> sProcesses;
};

ProcessThread* ProcessThread::sProcessThread = 0;
int ProcessThread::sProcessPipe[2];
Mutex ProcessThread::sProcessMutex;
std::map<pid_t, Process*> ProcessThread::sProcesses;

ProcessThread::ProcessThread()
{
    int flg;
    eintrwrap(flg, ::pipe(sProcessPipe));
    eintrwrap(flg, ::fcntl(sProcessPipe[1], F_GETFL, 0));
    eintrwrap(flg, ::fcntl(sProcessPipe[1], F_SETFL, flg | O_NONBLOCK));

#ifdef HAVE_SIGINFO
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = ProcessThread::processSignalHandler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    ::sigaction(SIGCHLD, &sa, 0);
#else
    ::signal(SIGCHLD, ProcessThread::processSignalHandler);
#endif
}

void ProcessThread::addPid(pid_t pid, Process* process)
{
    MutexLocker locker(&sProcessMutex);
    sProcesses[pid] = process;
}

void ProcessThread::removePid(pid_t pid)
{
    MutexLocker locker(&sProcessMutex);
    sProcesses.erase(pid);
}

void ProcessThread::stop()
{
    const pid_t stopPid = 1; // assume that no user process has a pid of 1
    sendPid(stopPid);
}

void ProcessThread::run()
{
    pid_t pid;
    char* buf = reinterpret_cast<char*>(&pid);
    ssize_t hasread = 0, r;
    for (;;) {
        //printf("reading pid (%lu remaining)\n", sizeof(pid_t) - hasread);
        r = ::read(sProcessPipe[0], &buf[hasread], sizeof(pid_t) - hasread);
        //printf("did read %ld\n", r);
        if (r >= 0)
            hasread += r;
        else {
            if (errno != EINTR) {
                error() << "ProcessThread is dying, errno " << errno << " strerror " << strerror(errno);
                break;
            }
        }
        if (hasread == sizeof(pid_t)) {
            //printf("got a full pid %d\n", pid);
            if (pid == 0) { // if our pid is 0 due to siginfo_t having an invalid si_pid then we have a misbehaving kernel.
                // regardless, we need to go through all children and call a non-blocking waitpid on each of them
                int ret;
                pid_t p;
                MutexLocker locker(&sProcessMutex);
                std::map<pid_t, Process*>::iterator proc = sProcesses.begin();
                const std::map<pid_t, Process*>::const_iterator end = sProcesses.end();
                while (proc != end) {
                    //printf("testing pid %d\n", proc->first);
                    p = ::waitpid(proc->first, &ret, WNOHANG);
                    switch(p) {
                    case 0:
                    case -1:
                        //printf("this is not the pid I'm looking for\n");
                        ++proc;
                        break;
                    default:
                        //printf("successfully waited for pid (got %d)\n", p);
                        if (WIFEXITED(ret))
                            ret = WEXITSTATUS(ret);
                        else
                            ret = -1;
                        Process *process = proc->second;
                        sProcesses.erase(proc++);
                        locker.unlock();
                        process->finish(ret);
                        locker.relock();
                    }
                }
            } else if (pid == 1) { // stopped
                break;
            } else {
                int ret;
                //printf("blocking wait pid %d\n", pid);
                ::waitpid(pid, &ret, 0);
                //printf("wait complete\n");
                Process *process = 0;
                {
                    MutexLocker locker(&sProcessMutex);
                    std::map<pid_t, Process*>::iterator proc = sProcesses.find(pid);
                    if (proc != sProcesses.end()) {
                        if (WIFEXITED(ret))
                            ret = WEXITSTATUS(ret);
                        else
                            ret = -1;
                        process = proc->second;
                        sProcesses.erase(proc);
                    }
                }
                if (process)
                    process->finish(ret);
            }
            hasread = 0;
        }
    }
    debug() << "ProcessThread dead";
    //printf("process thread died for some reason\n");
}

void ProcessThread::sendPid(pid_t pid)
{
    const char* buf = reinterpret_cast<const char*>(&pid);
    ssize_t written = 0, w;
    //printf("sending pid %d\n", pid);
    do {
        w = ::write(sProcessPipe[1], &buf[written], sizeof(pid_t) - written);
        if (w >= 0)
            written += w;
    } while ((static_cast<size_t>(written) < sizeof(pid_t))
             || (w == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)));
    //printf("sent pid %ld\n", written);
}

#ifdef HAVE_SIGINFO
void ProcessThread::processSignalHandler(int sig, siginfo_t* info, void*)
{
    assert(sig == SIGCHLD);
    (void)sig;
    sendPid(info->si_pid);
}
#else
void ProcessThread::processSignalHandler(int sig)
{
    assert(sig == SIGCHLD);
    (void)sig;
    sendPid(0);
}
#endif

void ProcessThread::installProcessHandler()
{
    assert(sProcessThread == 0);
    sProcessThread = new ProcessThread;
    sProcessThread->start();
}

Process::Process()
    : mPid(-1), mReturn(0), mStdInIndex(0), mStdOutIndex(0), mStdErrIndex(0), mMode(Sync)
{
    pthread_once(&sProcessHandler, ProcessThread::installProcessHandler);

    mStdIn[0] = mStdIn[1] = -1;
    mStdOut[0] = mStdOut[1] = -1;
    mStdErr[0] = mStdErr[1] = -1;
    mSync[0] = mSync[1] = -1;
}

Process::~Process()
{
    if (mPid != -1)
        ProcessThread::removePid(mPid);

    if (mStdIn[0] != -1) {
        // try to finish off any pending writes
        handleInput(mStdIn[1]);
    }

    closeStdIn();
    closeStdOut();
    closeStdErr();

    int w;
    if (mSync[0] != -1)
        eintrwrap(w, ::close(mSync[0]));
    if (mSync[1] != -1)
        eintrwrap(w, ::close(mSync[1]));
}

void Process::setCwd(const Path& cwd)
{
    mCwd = cwd;
}

Path Process::findCommand(const String& command)
{
    if (command.isEmpty() || command.at(0) == '/')
        return command;

    const char* path = getenv("PATH");
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

Process::ExecState Process::startInternal(const String& command, const List<String>& a, const List<String>& environ,
                                          int timeout, unsigned execFlags)
{
    mErrorString.clear();

    Path cmd = findCommand(command);
    if (cmd.isEmpty()) {
        mErrorString = "Command not found";
        return Error;
    }
    List<String> arguments = a;
#if 0
    char *contents;
    const int read = cmd.readAll(contents, 33);

    contents[std::min(read, 32)] = '\0';
    if (read > 2 && !strncmp(contents, "#!", 2)) {
        char *newLine = strchr(contents, '\n');
        if (newLine) {
            arguments.prepend(command);
            cmd = findCommand(String(contents + 2, newLine - contents - 2));
        }
    }
#endif
    int err;

    eintrwrap(err, ::pipe(mStdIn));
    eintrwrap(err, ::pipe(mStdOut));
    eintrwrap(err, ::pipe(mStdErr));
    if (mMode == Sync)
        eintrwrap(err, ::pipe(mSync));

    const char **args = new const char*[arguments.size() + 2];
    // const char* args[arguments.size() + 2];
    args[arguments.size() + 1] = 0;
    args[0] = cmd.nullTerminated();
    int pos = 1;
    for (List<String>::const_iterator it = arguments.begin(); it != arguments.end(); ++it) {
        args[pos] = it->nullTerminated();
        //printf("arg: '%s'\n", args[pos]);
        ++pos;
    }

    const bool hasEnviron = !environ.empty();

    const char **env = new const char*[environ.size() + 1];
    env[environ.size()] = 0;

    if (hasEnviron) {
        pos = 0;
        //printf("fork, about to exec '%s'\n", cmd.nullTerminated());
        for (List<String>::const_iterator it = environ.begin(); it != environ.end(); ++it) {
            env[pos] = it->nullTerminated();
            //printf("env: '%s'\n", env[pos]);
            ++pos;
        }
    }

    mPid = ::fork();
    if (mPid == -1) {
        //printf("fork, something horrible has happened %d\n", errno);
        // bail out
        eintrwrap(err, ::close(mStdIn[1]));
        eintrwrap(err, ::close(mStdIn[0]));
        eintrwrap(err, ::close(mStdOut[1]));
        eintrwrap(err, ::close(mStdOut[0]));
        eintrwrap(err, ::close(mStdErr[1]));
        eintrwrap(err, ::close(mStdErr[0]));
        mErrorString = "Fork failed";
        delete[] env;
        delete[] args;
        return Error;
    } else if (mPid == 0) {
        //printf("fork, in child\n");
        // child, should do some error checking here really
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

        if (!mCwd.isEmpty())
            eintrwrap(err, ::chdir(mCwd.nullTerminated()));
        int ret;
        if (hasEnviron)
            ret = ::execve(cmd.nullTerminated(), const_cast<char* const*>(args), const_cast<char* const*>(env));
        else
            ret = ::execv(cmd.nullTerminated(), const_cast<char* const*>(args));
        ::_exit(1);
        (void)ret;
        //printf("fork, exec seemingly failed %d, %d %s\n", ret, errno, strerror(errno));
    } else {
        delete[] env;
        delete[] args;

        ProcessThread::addPid(mPid, this);

        // parent
        eintrwrap(err, ::close(mStdIn[0]));
        eintrwrap(err, ::close(mStdOut[1]));
        eintrwrap(err, ::close(mStdErr[1]));

        //printf("fork, in parent\n");

        int flags;
        eintrwrap(flags, fcntl(mStdIn[1], F_GETFL, 0));
        eintrwrap(flags, fcntl(mStdIn[1], F_SETFL, flags | O_NONBLOCK));
        eintrwrap(flags, fcntl(mStdOut[0], F_GETFL, 0));
        eintrwrap(flags, fcntl(mStdOut[0], F_SETFL, flags | O_NONBLOCK));
        eintrwrap(flags, fcntl(mStdErr[0], F_GETFL, 0));
        eintrwrap(flags, fcntl(mStdErr[0], F_SETFL, flags | O_NONBLOCK));

        //printf("fork, about to add fds: stdin=%d, stdout=%d, stderr=%d\n", mStdIn[1], mStdOut[0], mStdErr[0]);
        if (mMode == Async) {
            if (EventLoop *loop = EventLoop::instance()) {
                loop->addFileDescriptor(mStdOut[0], EventLoop::Read, processCallback, this);
                loop->addFileDescriptor(mStdErr[0], EventLoop::Read, processCallback, this);
            }
        } else {
            // select and stuff
            timeval started, now, *selecttime = 0;
            if (timeout > 0) {
                Rct::gettime(&started);
                now = started;
                selecttime = &now;
                Rct::timevalAdd(selecttime, timeout);
            }
            if (!(execFlags & NoCloseStdIn))
                closeStdIn();
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
                const int ret = select(max + 1, &rfds, &wfds, 0, selecttime);
                if (ret == -1) { // ow
                    mErrorString = "Sync select failed";
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
                        MutexLocker lock(&mMutex);
                        assert(mPid == -1);
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
                    mFinished(this);
                    return Done;
                }
                if (timeout) {
                    assert(selecttime);
                    Rct::gettime(selecttime);
                    const int lasted = Rct::timevalDiff(selecttime, &started);
                    if (lasted >= timeout) {
                        // timeout, we're done
                        stop(); // attempt to kill
                        return TimedOut;
                    }
                    *selecttime = started;
                    Rct::timevalAdd(selecttime, timeout);
                }
            }
        }
    }
    return Done;
}

bool Process::start(const String& command, const List<String>& a, const List<String>& environ)
{
    mMode = Async;
    return startInternal(command, a, environ) == Done;
}

Process::ExecState Process::exec(const String& command, const List<String>& arguments, int timeout, unsigned flags)
{
    mMode = Sync;
    return startInternal(command, arguments, List<String>(), timeout, flags);
}

Process::ExecState Process::exec(const String& command, const List<String>& a, const List<String>& environ,
                                 int timeout, unsigned flags)
{
    mMode = Sync;
    return startInternal(command, a, environ, timeout, flags);
}

void Process::write(const String& data)
{
    if (!data.isEmpty() && mStdIn[1] != -1) {
        mStdInBuffer.push_back(data);
        handleInput(mStdIn[1]);
    }
}

void Process::closeStdIn()
{
    if (mStdIn[1] == -1)
        return;

    EventLoop::instance()->removeFileDescriptor(mStdIn[1]);
    int err;
    eintrwrap(err, ::close(mStdIn[1]));
    mStdIn[1] = -1;
}

void Process::closeStdOut()
{
    if (mStdOut[0] == -1)
        return;

    EventLoop::instance()->removeFileDescriptor(mStdOut[0]);
    int err;
    eintrwrap(err, ::close(mStdOut[0]));
    mStdOut[0] = -1;
}

void Process::closeStdErr()
{
    if (mStdErr[0] == -1)
        return;

    EventLoop::instance()->removeFileDescriptor(mStdErr[0]);
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

void Process::processCallback(int fd, unsigned int flags, void* userData)
{
    Process* proc = reinterpret_cast<Process*>(userData);
    if (fd == proc->mStdIn[1])
        proc->handleInput(fd);
    else if (fd == proc->mStdOut[0])
        proc->handleOutput(fd, proc->mStdOutBuffer, proc->mStdOutIndex, proc->mReadyReadStdOut);
    else if (fd == proc->mStdErr[0])
        proc->handleOutput(fd, proc->mStdErrBuffer, proc->mStdErrIndex, proc->mReadyReadStdErr);
}

void Process::finish(int returnCode)
{
    {
        MutexLocker lock(&mMutex);
        mPid = -1;
        mReturn = returnCode;

        mStdInBuffer.clear();
        closeStdIn();

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
    }

    if (mMode == Async)
        mFinished(this);
}

void Process::handleInput(int fd)
{
    assert(EventLoop::instance());
    EventLoop::instance()->removeFileDescriptor(fd);

    //static int ting = 0;
    //printf("Process::handleInput (cnt=%d)\n", ++ting);
    for (;;) {
        if (mStdInBuffer.empty())
            return;

        //printf("Process::handleInput in loop\n");
        int w, want;
        const String& front = mStdInBuffer.front();
        if (mStdInIndex) {
            want = front.size() - mStdInIndex;
            eintrwrap(w, ::write(fd, front.mid(mStdInIndex).constData(), want));
        } else {
            want = front.size();
            eintrwrap(w, ::write(fd, front.constData(), want));
        }
        if (w == -1) {
            EventLoop::instance()->addFileDescriptor(fd, EventLoop::Write, processCallback, this);
            break;
        } else if (w == want) {
            mStdInBuffer.pop_front();
            mStdInIndex = 0;
        } else
            mStdInIndex += w;
    }
}

void Process::handleOutput(int fd, String& buffer, int& index, signalslot::Signal1<Process*>& signal)
{
    //printf("Process::handleOutput %d\n", fd);
    enum { BufSize = 1024, MaxSize = (1024 * 1024 * 16) };
    char buf[BufSize];
    int total = 0;
    for (;;) {
        int r;
        eintrwrap(r, ::read(fd, buf, BufSize));
        if (r == -1) {
            //printf("Process::handleOutput %d returning -1, errno %d %s\n", fd, errno, strerror(errno));
            break;
        } else if (r == 0) { // file descriptor closed, remove it
            //printf("Process::handleOutput %d returning 0\n", fd);
            EventLoop::instance()->removeFileDescriptor(fd);
            break;
        } else {
            //printf("Process::handleOutput in loop %d\n", fd);
            //printf("data: '%s'\n", std::string(buf, r).c_str());
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

    //printf("total data '%s'\n", buffer.nullTerminated());

    if (total)
        signal(this);
}

void Process::stop()
{
    if (mPid == -1)
        return;

    ::kill(mPid, SIGTERM);
}

List<String> Process::environment()
{
#ifdef OS_Darwin
    char **cur = *_NSGetEnviron();
#else
    extern char** environ;
    char** cur = environ;
#endif
    List<String> env;
    while (*cur) {
        env.push_back(*cur);
        ++cur;
    }
    return env;
}
