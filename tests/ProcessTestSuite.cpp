#include "ProcessTestSuite.h"

#include <rct/Process.h>
#include <thread>
#include <mutex>
#include "UdpIncludes.h"

void ProcessTestSuite::setUp()
{
    int res;

#ifdef _WIN32
    // On Windows, we need to setup Windows socket before using it.
    // Calling this function multiple times is ok, according to msdn.
    WSADATA unused;
    CHECK_RETURN(WSAStartup(MAKEWORD(2,2), &unused) != 0, "WSAStartup()");
#endif

    // create udp sockets for communication with ChildProcess
    listenSock = socket(AF_INET, SOCK_DGRAM, 0);
    sendSock = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK_RETURN(listenSock == INVALID_SOCKET || sendSock == INVALID_SOCKET, "socket()");

    // setting SO_REUSEADDR will allow us to reclaim the socket shortly after the socket
    // has been closed by another process (possibly from the same executable).
    // This way, we can call the test suite in quick succession.
    const int optval = 1;
    res = setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (optval_p)&optval,
                     sizeof(optval));
    CHECK_RETURN(res != 0, "setsockopt(SO_REUSEADDR)");

    // set timeout on reads
#ifdef _WIN32
    timeout_t to = recvTimeout_ms;
#else
    timeout_t to;
    to.tv_sec = recvTimeout_ms / 1000;
    to.tv_usec = (recvTimeout_ms % 1000) * 1000;
#endif
    res = setsockopt(listenSock, SOL_SOCKET, SO_RCVTIMEO, (optval_p)&to,
                     sizeof(timeout_t));
    CHECK_RETURN(res != 0, "setsockopt(SO_RCVTIMEO)");

    // Bind to LOOPBACK (localhost, 127.0.0.1) and the specified port so that we
    // receive all udp data sent to it.
    sockaddr_in listenAddr;
    std::memset(&listenAddr, 0, sizeof(listenAddr));
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); //only accept data from localhost
    listenAddr.sin_port = htons(listenPort);
    res = bind(listenSock, (sockaddr*)&listenAddr, sizeof(listenAddr));
    CHECK_RETURN(res < 0, "bind");
}

std::string ProcessTestSuite::udp_recv()
{
    static const size_t BUFFER_SIZE = 80;
    char buf[BUFFER_SIZE];
    ssize_t recvSize = recv(listenSock, buf, BUFFER_SIZE, 0);
    if(recvSize < 0)
    {
        // This might simply be a read timeout (and not an actual error)
#ifdef _WIN32
        const DWORD errorCode = WSAGetLastError();
        const bool reallyAnError = errorCode != WSAETIMEDOUT;
#else
        const int errorCode = errno;
        const bool reallyAnError = errorCode != EAGAIN && errorCode != EWOULDBLOCK;
#endif
        if(reallyAnError)
        {
            std::cout << "Error reading from udp pipe: " << errorCode << std::endl;
            exit(-1);
        }
    }

    if(recvSize < 0) recvSize = 0;
    std::string ret(buf, recvSize);
    return ret;
}

void ProcessTestSuite::udp_send(const std::string &data)
{
    sockaddr_in sendAddr;
    std::memset(&sendAddr, 0, sizeof(sendAddr));
    sendAddr.sin_family = AF_INET;
    sendAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sendAddr.sin_port = htons(sendPort);

    int res = sendto(sendSock, data.data(), data.size(), 0,
                     (sockaddr*)&sendAddr, sizeof(sendAddr));

    CHECK_RETURN((unsigned)res != data.size(), "Error sending UDP");
}

void ProcessTestSuite::tearDown()
{
#ifdef _WIN32
    closesocket(listenSock);
    closesocket(sendSock);
    WSACleanup();
#else
    close(listenSock);
    close(sendSock);
#endif
    usleep(100 * 1000);
}

void ProcessTestSuite::realSleep(int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    timespec s;
    s.tv_sec = ms/1000;
    s.tv_nsec = (ms%1000) * 1000 * 1000;

    while(nanosleep(&s, &s) == -1 && errno == EINTR);
#endif
}

class CallOnScopeExit
{
public:
    CallOnScopeExit(const std::function<void()> &f) : m_f(f) {}
    ~CallOnScopeExit() {m_f();}
private:
    const std::function<void()> m_f;
};

void ProcessTestSuite::returnCode()
{
    // tell child process to exit with code 12 in 50 ms from now
    std::thread t([this](){realSleep(50); udp_send("exit 12");});

    // start process
    Process p;
    p.exec("ChildProcess");  // synchronous call

    t.join();
    realSleep(50);

    // check exit code
    CPPUNIT_ASSERT(p.returnCode() == 12);
}

void ProcessTestSuite::startAsync()
{
    Process p;
    p.start("ChildProcess");  // asynchronous call
    CPPUNIT_ASSERT(!p.isFinished());
    realSleep(50);
    CPPUNIT_ASSERT(!p.isFinished());
    udp_send("exit 1");
    realSleep(50);
    CPPUNIT_ASSERT(p.isFinished());
    CPPUNIT_ASSERT(p.returnCode() == 1);
}

void ProcessTestSuite::readFromStdout()
{
    Process p;
    std::vector<String> dataReadFromStdout;

    // execute ChildProcess synchronously, but in another thread, so
    // that we can monitor its stdout in the main thread.
    // Note: Calling ChildProcess asynchronously with Process::start()
    // and polling for stdout does not work because an asynchronous
    // Process requires a running EventLoop.
    std::thread t([&](){p.exec("ChildProcess");});
    CallOnScopeExit joiner([&](){if(t.joinable()) t.join();});

    realSleep(50);
    CPPUNIT_ASSERT(!p.isFinished());
    dataReadFromStdout.push_back(p.readAllStdOut());  // should be empty
    udp_send("stdout This is a test");
    realSleep(50);
    dataReadFromStdout.push_back(p.readAllStdOut());  // should be "This is a test"
    dataReadFromStdout.push_back(p.readAllStdOut());  // should be empty
    realSleep(50);
    dataReadFromStdout.push_back(p.readAllStdOut());  // should be empty
    udp_send("exit 0");
    realSleep(50);
    dataReadFromStdout.push_back(p.readAllStdOut());  // should be empty

    CPPUNIT_ASSERT(p.isFinished());

    // need to do the test *after* ChildProcess terminates, because
    // if the check fails, we exit this scope, destroying the Process object
    // before ChildProcess exits.

    CPPUNIT_ASSERT(dataReadFromStdout[0].isEmpty());
    CPPUNIT_ASSERT(dataReadFromStdout[1] == "This is a test");
    CPPUNIT_ASSERT(dataReadFromStdout[2].isEmpty());
    CPPUNIT_ASSERT(dataReadFromStdout[3].isEmpty());
    CPPUNIT_ASSERT(dataReadFromStdout[4].isEmpty());
    t.join();
}

void ProcessTestSuite::readFromStderr()
{
    Process p;
    std::vector<String> dataReadFromStderr;

    // execute ChildProcess synchronously, but in another thread, so
    // that we can monitor its stdout in the main thread.
    // Note: Calling ChildProcess asynchronously with Process::start()
    // and polling for stdout does not work because an asynchronous
    // Process requires a running EventLoop.
    std::thread t([&](){p.exec("ChildProcess");});
    CallOnScopeExit joiner([&](){if(t.joinable()) t.join();});

    realSleep(50);
    bool isFinished1 = p.isFinished();
    dataReadFromStderr.push_back(p.readAllStdErr());  // should be empty
    udp_send("stderr This is a stderr test");
    realSleep(50);
    dataReadFromStderr.push_back(p.readAllStdErr());  // should be "This is a stderr test"
    dataReadFromStderr.push_back(p.readAllStdErr());  // should be empty
    realSleep(50);
    dataReadFromStderr.push_back(p.readAllStdErr());  // should be empty
    udp_send("exit 0");
    realSleep(50);
    dataReadFromStderr.push_back(p.readAllStdErr());  // should be empty
    bool isFinished2 = p.isFinished();

    // need to do the test *after* ChildProcess terminates, because
    // if a check fails, we exit this scope, destroying the Process object
    // before ChildProcess exits (that's a design weakness in the non-windows Process
    // implementation).
    CPPUNIT_ASSERT(!isFinished1);
    CPPUNIT_ASSERT(dataReadFromStderr[0].isEmpty());
    CPPUNIT_ASSERT(dataReadFromStderr[1] == "This is a stderr test");
    CPPUNIT_ASSERT(dataReadFromStderr[2].isEmpty());
    CPPUNIT_ASSERT(dataReadFromStderr[3].isEmpty());
    CPPUNIT_ASSERT(dataReadFromStderr[4].isEmpty());
    CPPUNIT_ASSERT(isFinished2);
}

void ProcessTestSuite::signals()
{
    std::shared_ptr<EventLoop> loop(new EventLoop);
    loop->init(EventLoop::MainEventLoop);

    Process p;
    bool finishedCalled = false;
    bool wrongProcessObjPassed = false;
    std::string stdoutData, stderrData;
    std::mutex mut;

    p.readyReadStdOut().connect([&](Process* pp)
                                {
                                    std::lock_guard<std::mutex> lock(mut);
                                    if(pp != &p) wrongProcessObjPassed = true;
                                    String data = pp->readAllStdOut();
                                    stdoutData.append(data.c_str());
                                });

    p.readyReadStdErr().connect([&](Process* pp)
                                {
                                    std::lock_guard<std::mutex> lock(mut);
                                    if(pp != &p) wrongProcessObjPassed = true;
                                    stderrData.append(pp->readAllStdErr().c_str());
                                });

    p.finished().connect([&](Process* pp, pid_t)
                         {
                             std::lock_guard<std::mutex> lock(mut);
                             if(pp != &p) wrongProcessObjPassed = true;
                             finishedCalled = true;
                         });

    p.start("ChildProcess");  // start asynchronously

    std::thread t([this]()
        {
            realSleep(50);
            udp_send("stdout Hello world");
            udp_send("stderr Error world");
            realSleep(50);
            udp_send("exit 0");
            realSleep(50);
        });
    CallOnScopeExit joiner([&](){if(t.joinable()) t.join();});

    // Signals are deliviered by the running event loop.
    loop->exec(200);
    t.join();

    CPPUNIT_ASSERT(stdoutData == "Hello world");
    CPPUNIT_ASSERT(stderrData == "Error world");
    CPPUNIT_ASSERT(!wrongProcessObjPassed);
    CPPUNIT_ASSERT(finishedCalled);
}

void ProcessTestSuite::execTimeout()
{
    Process p;
    auto res = p.exec("ChildProcess", List<String>(), 200);  // timeout: 200 ms
    CPPUNIT_ASSERT(res == Process::TimedOut);
    CPPUNIT_ASSERT(p.isFinished());
    CPPUNIT_ASSERT(p.returnCode() == Process::ReturnKilled);
    CPPUNIT_ASSERT(p.errorString() == "Timed out");
}

void ProcessTestSuite::env()
{
    Process p;

    List<String> env = Process::environment();
    env.push_back("TESTVALUE=foo");   // add custom env

    std::thread t([&](){p.exec("ChildProcess", List<String>(), env);});

    realSleep(50);
    udp_send("getEnv");
    realSleep(50);
    String readEnv = p.readAllStdOut();
    udp_send("exit 0");

    t.join();

    CPPUNIT_ASSERT(readEnv.contains("TESTVALUE=foo"));
}

void ProcessTestSuite::writeToStdin()
{
    std::shared_ptr<EventLoop> loop(new EventLoop);
    loop->init(EventLoop::MainEventLoop);

    Process p;
    std::string whatWeRead;

    p.start("ChildProcess");

    std::thread t([&]()
                  {
                      realSleep(50);
                      p.write("stdin write test");
                      realSleep(50);
                      whatWeRead = udp_recv();
                      udp_send("exit 0");
                      realSleep(50);
                  });

    loop->exec(200);

    t.join();

    CPPUNIT_ASSERT(p.isFinished());
    CPPUNIT_ASSERT(whatWeRead == "stdin write test");
}

void ProcessTestSuite::startNonExistingProgram()
{
    Process p;
    CPPUNIT_ASSERT(!p.start("ThisProgramDoesNotExist"));  // async
}

void ProcessTestSuite::startUnicodeProgram()
{
    Process p;
    CPPUNIT_ASSERT(p.start(u8"ChildProcess_Äßéמש最終"));  // async
    CPPUNIT_ASSERT(!p.isFinished());
    realSleep(50);
    CPPUNIT_ASSERT(!p.isFinished());
    udp_send("exit 1");
    realSleep(50);
    CPPUNIT_ASSERT(p.isFinished());
    CPPUNIT_ASSERT(p.returnCode() == 1);
}

void ProcessTestSuite::commandLineArgs()
{
    List<String> args;
    args.push_back("Arg1");
    args.push_back("Arg2 with space");
    args.push_back("Arg3\nwith\nnewline");
    Process p;

    // execute ChildProcess synchronously, but in another thread, so
    // that we can monitor its stdout in the main thread.
    // Note: Calling ChildProcess asynchronously with Process::start()
    // and polling for stdout does not work because an asynchronous
    // Process requires a running EventLoop (on windows, that's not req'd)
    std::thread t([&](){p.exec("ChildProcess", args);});
    CallOnScopeExit joiner([&](){if(t.joinable()) t.join();});

    realSleep(50);
    udp_send("getArgv");
    realSleep(50);
    String data = p.readAllStdOut();
    udp_send("exit 0");

    data.replace("\r", "");   // change \r\n to \n for comparison
    List<String> gotArgs = data.split('\0');

    // includes argv[0] which is empty, and two trailing entries.
    CPPUNIT_ASSERT(gotArgs.size() == 6);
    CPPUNIT_ASSERT(gotArgs[1] == "\nArg1");
    CPPUNIT_ASSERT(gotArgs[2] == "\nArg2 with space");
    CPPUNIT_ASSERT(gotArgs[3] == "\nArg3\nwith\nnewline");
    CPPUNIT_ASSERT(gotArgs[4] == "\n");
    CPPUNIT_ASSERT(gotArgs[5] == "\n");
}

#ifdef _WIN32
void ProcessTestSuite::killWindows()
{
    Process p;
    p.start("ChildProcess");

    realSleep(50);
    CPPUNIT_ASSERT(!p.isFinished());
    p.kill();
    realSleep(50);
    CPPUNIT_ASSERT(p.isFinished());
    CPPUNIT_ASSERT(p.returnCode() == Process::ReturnKilled);
}

void ProcessTestSuite::destructorWindows()
{
    std::unique_ptr<Process> p(new Process);

    p->start("ChildProcess");
    realSleep(50);
    CPPUNIT_ASSERT(!p->isFinished());
    p.reset();   //deletes p

    // There should not have been a crash
    CPPUNIT_ASSERT(true);
}
#endif // _WIN32
