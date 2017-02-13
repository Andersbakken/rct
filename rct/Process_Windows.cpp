/**
 * @file Process_Windows.cpp
 *
 * Containts the implementation for the Process class on Windows.
 */

#ifndef _WIN32
#  error "This file can only be built on Windows. On other OSs, build Process.cpp instead"
#endif

#include "Process.h"
#include "Log.h"
#include "WindowsUnicodeConversion.h"

#include <chrono>

Process::Process()
    : mMode(Sync), mReturn(ReturnUnset)
{
    for(int i=0; i<NUM_HANDLES; i++)
    {
        mStdIn[i]  = mStdOut[i] = mStdErr[i] = INVALID_HANDLE_VALUE;
    }

    mProcess.dwProcessId = -1;
    mProcess.hThread = mProcess.hProcess = INVALID_HANDLE_VALUE;
}

Process::~Process()
{
    waitForProcessToFinish();

    if(mthStdout.joinable()) mthStdout.join();
    if(mthStderr.joinable()) mthStderr.join();
    if(mthManageTimeout.joinable()) mthManageTimeout.join();
}

/*static*/ void Process::closeHandleIfValid(HANDLE &f_handle)
{
    if(f_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(f_handle);
        f_handle = INVALID_HANDLE_VALUE;
    }
}

Process::ExecState Process::exec(const Path &f_cmd,
                                 const List<String> &f_args,
                                 int f_timeout_ms, unsigned int f_flags)
{
    mMode = Sync;
    auto ret = startInternal(f_cmd, f_args, List<String>(), f_timeout_ms, f_flags);

    if(mthStdout.joinable()) mthStdout.join();
    if(mthStderr.joinable()) mthStderr.join();
    if(mthManageTimeout.joinable()) mthManageTimeout.join();

    if(mProcessTimeoutStatus == KILLED)
    {
        mErrorString = "Timed out";
        return TimedOut;
    }

    return ret;
}

bool Process::start(const Path &f_cmd,
                    const List<String> &f_args,
                    const List<String> &f_environ)
{
    mMode = Async;
    return startInternal(f_cmd, f_args, f_environ) == Done;
}

Process::ExecState Process::startInternal(const Path &f_cmd, const List<String> &f_args,
                            const List<String> &f_environ, int f_timeout_ms,
                            unsigned int f_flags)
{
    // unused arguments (for now)
    (void) f_args;
    (void) f_environ;
    (void) f_flags;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(saAttr);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    // Create anonymous pipes which we will use
    if(!CreatePipe(&mStdIn [READ_END], &mStdIn [WRITE_END], &saAttr, 0) ||
       !CreatePipe(&mStdOut[READ_END], &mStdOut[WRITE_END], &saAttr, 0) ||
       !CreatePipe(&mStdErr[READ_END], &mStdErr[WRITE_END], &saAttr, 0))
    {
        error() << "Error creating pipes";
        return Error;
    }

    // the child is not supposed to gain access to the pipes' parent end
    if(!SetHandleInformation(mStdIn[WRITE_END], HANDLE_FLAG_INHERIT, 0) ||
       !SetHandleInformation(mStdOut[READ_END], HANDLE_FLAG_INHERIT, 0) ||
       !SetHandleInformation(mStdErr[READ_END], HANDLE_FLAG_INHERIT, 0))
    {
        error() << "SetHandleInformation: " << GetLastError();
        return Error;
    }

    // set up STARTUPINFO structure. It tells CreateProcess to use the pipes
    // we just created as stdin, stdout and stderr for the new process.
    STARTUPINFO siStartInfo;
    memset(&siStartInfo, 0, sizeof(siStartInfo));
    siStartInfo.cb = sizeof(siStartInfo);
    siStartInfo.hStdInput  = mStdIn[READ_END];
    siStartInfo.hStdOutput = mStdOut[WRITE_END];
    siStartInfo.hStdError  = mStdErr[WRITE_END];
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    memset(&mProcess, 0, sizeof(mProcess));

    // CreateProcess takes a non-const pointer as the command, so we need
    // to create a copy.
    std::string nonConstCommand(f_cmd);

    if(!CreateProcess(NULL,  // application name: we pass it through lpCommandLine
                      &nonConstCommand[0],
                      NULL,  // security attrs
                      NULL,  // thread security attrs
                      TRUE,  // handles are inherited
                      0,     // creation flags
                      NULL,  // TODO: env
                      NULL,  // TODO: cwd
                      &siStartInfo,  // in: stdin, stdout, stderr pipes
                      &mProcess    // out: info about the new process
                             ))
    {
        error() << "Error in CreateProcess(): " << GetLastError();
        return Error;
    }

    // we need to close our handles to the write end of these pipe. Otherwise,
    // ReadFile() will not return when the child process terminates.
    closeHandleIfValid(mStdOut[WRITE_END]);
    closeHandleIfValid(mStdErr[WRITE_END]);
    closeHandleIfValid(mStdIn[READ_END]);

    // We start one thread for each pipe (child process' stdout and child process'
    // stderr). We could use overlapping IO here, but it's very complicated and
    // probably doesn't do what we want exactly, so we stick with the (somewhat
    // ugly) two-thread solution.

    // setup timeout
    if(f_timeout_ms > 0)
    {
        // setup condition variable. The condition variable will be signalled when the
        // process finishes on its own, telling the timeout process to quit.
        // When the child process does not finish in time, std::wait_condition::wait_for()
        // will timeout. Then, the manageTimeout() thread will kill the child process.

        mProcessTimeoutStatus = NOT_FINISHED;
        mthManageTimeout = std::thread(&Process::manageTimeout, this, f_timeout_ms);
    }
    else
    {
        mProcessTimeoutStatus = NO_TIMEOUT;
    }

    mthStdout = std::thread(&Process::readFromPipe, this, STDOUT);
    mthStderr = std::thread(&Process::readFromPipe, this, STDERR);

    return Done;
}

void Process::readFromPipe(PipeToReadFrom f_pipe)
{
    // This method runs in an extra thread.
    // It continuously reads from the supplied f_pipe (either the child process' stdout
    // or the child process' stderr).
    // The reads are blocking, that's why we do both reads in an extra thread.
    // Once we receive something, we add it to the appropriate buffer and signal the
    // reception.
    // When the pipe is broken (i.e., when the child terminates), the stdout thread does
    // all the cleanup.

    // shorter names:
    HANDLE &inPipe          = (f_pipe == STDOUT ? mStdOut[READ_END] : mStdErr[READ_END]);
    String &outBuffer       = (f_pipe == STDOUT ? mStdOutBuffer : mStdErrBuffer);
    SignalOnData &outSignal = (f_pipe == STDOUT ? mReadyReadStdOut : mReadyReadStdErr);

    CHAR buf[PIPE_READ_BUFFER_SIZE];
    DWORD bytesRead = 0;

    bool moreToRead = true;

    while(moreToRead)
    {
        if(ReadFile(inPipe, buf, PIPE_READ_BUFFER_SIZE, &bytesRead, NULL))
        {
            {
                std::lock_guard<std::mutex> lo(mMutex);
                outBuffer.append(buf, bytesRead);
            }

            outSignal(this);
        }
        else
        {
            const DWORD err = GetLastError();

            if(err == ERROR_BROKEN_PIPE)
            {
                // child process terminated (this is not an error)
            }
            else
            {
                error() << "Error while reading from child process: " << err;
            }

            moreToRead = false;
        }
    }

    // cleanup in extra function.
    if(f_pipe == STDOUT) waitForProcessToFinish();
}

void Process::waitForProcessToFinish()
{
    if(mProcess.hProcess == INVALID_HANDLE_VALUE) return;  // already finished.

    DWORD res = WaitForSingleObject(mProcess.hProcess, INFINITE);

    std::unique_lock<std::mutex> lock(mMutex);
    if(res != WAIT_OBJECT_0)
    {
        error() << "Error waiting for process to finish: " << res;
    }

    // stop the timeout thread (if there is one)
    if(mProcessTimeoutStatus == NOT_FINISHED)
    {
        mProcessTimeoutStatus = FINISHED_ON_ITS_OWN;
        mProcessFinished_cond.notify_all();
        // the notified thread will not wake up until we release the mutex.
    }

    // store exit code
    DWORD retCode;
    GetExitCodeProcess(mProcess.hProcess, &retCode);
    mReturn = retCode;

    // Close remaining handles so that the OS can clean up
    for(int i=0; i<NUM_HANDLES; i++)
    {
        closeHandleIfValid(mStdIn[i]);
        closeHandleIfValid(mStdOut[i]);
        closeHandleIfValid(mStdErr[i]);
    }

    closeHandleIfValid(mProcess.hThread);
    closeHandleIfValid(mProcess.hProcess);

    lock.unlock();

    // send 'finished' signal
    mFinished(this);
}

void Process::manageTimeout(int timeout_ms)
{
    std::unique_lock<std::mutex> lock(mMutex);
    if(mProcessFinished_cond.wait_for(
           lock,
           std::chrono::milliseconds(timeout_ms),
           [this](){return mProcessTimeoutStatus == FINISHED_ON_ITS_OWN;}))
    {
        // Process finished on its own, no need to kill it here
    }
    else
    {
        // Process did not finish in time. We need to kill it.
        TerminateProcess(mProcess.hProcess, ReturnKilled);
        mProcessTimeoutStatus = KILLED;
    }
}

void Process::write(const String &f_data)
{
    std::lock_guard<std::mutex> lock(mStdinMutex);
    if(!WriteFile(mStdIn[WRITE_END], f_data.data(), f_data.size(), NULL, NULL))
    {
        const DWORD errorCode = GetLastError();

        switch(errorCode)
        {
        case ERROR_BROKEN_PIPE:
            // Child process terminated before it could read our data.
            // Maybe the process was killed due to a timeout?
            // Anyway, this is not an error.
            break;
        default:
            error() << "Error writing to child process' stdin. GetLastError()="
                    << errorCode;
        }
    }
}

/*static*/ List<String> Process::environment()
{
    List<String> ret;

    /// RAII class to make sure that the LPTCH object is correctly cleared.
    class Env_Raii
    {
    public:
        Env_Raii(wchar_t *env) : m_data(env) {}
        ~Env_Raii() {FreeEnvironmentStringsW(m_data);}
        wchar_t const *data() const {return m_data;}
    private:
        wchar_t * const m_data;
    };

    Env_Raii envData(GetEnvironmentStringsW());

    wchar_t const * readPtr = envData.data();
    for(;;)
    {
        String newEntry = static_cast<const char*>(Utf16To8(readPtr));
        readPtr += wcslen(readPtr);  // readPtr now points at entry terminating \0
        readPtr++;   // readPtr now pointers at the beginning of the next entry.

        if(newEntry.size() == 0) break;
        if(newEntry[0] == '=') continue; // remove spurious entry

        ret.push_back(newEntry);
    }

    return ret;
}

int Process::returnCode() const
{
     std::lock_guard<std::mutex> lock(mMutex);
     return mReturn;
}

bool Process::isFinished() const
{
    return mProcess.hProcess == INVALID_HANDLE_VALUE;
}

String Process::readAllStdOut()
{
    std::lock_guard<std::mutex> lock(mMutex);
    String ret = mStdOutBuffer;
    mStdOutBuffer.clear();
    return ret;
}

String Process::readAllStdErr()
{
    std::lock_guard<std::mutex> lock(mMutex);
    String ret = mStdErrBuffer;
    mStdErrBuffer.clear();
    return ret;
}

String Process::errorString() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mErrorString;
}
