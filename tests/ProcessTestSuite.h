#include <cppunit/extensions/HelperMacros.h>

#include "UdpIncludes.h"

class ProcessTestSuite : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(ProcessTestSuite);

    CPPUNIT_TEST(returnCode);
#ifndef _WIN32
    CPPUNIT_TEST(startAsync);
    CPPUNIT_TEST(readFromStdout);
    CPPUNIT_TEST(readFromStderr);
    CPPUNIT_TEST(signals);
    CPPUNIT_TEST(execTimeout);
    CPPUNIT_TEST(env);
    CPPUNIT_TEST(writeToStdin);
#endif
    CPPUNIT_TEST_SUITE_END();

    public:
        void setUp();
        void tearDown();

    static const int recvTimeout_ms = 100;

protected:
    // start a process and examine its return code
    void returnCode();
#ifndef _WIN32
    // start a process asynchronously
    void startAsync();

    // read data from stdout while the child process is running
    void readFromStdout();

    // read data from stderr while the child process is running
    void readFromStderr();

    // test whether Process sends the correct signals through EventLoop
    // when the child process writes stuff to stdout, stderr or when it
    // exits.
    void signals();

    // Start a process that does not terminate in the requested time.
    // Process::exec() should return after the timeout.
    void execTimeout();

    // Test whether setting the environment for the child process works.
    void env();

    void writeToStdin();
#endif
public:
    /**
     * A version of sleep that is not interrupted by signals and has
     * millisecond resolution.
     */
    static void realSleep(int ms);

private:
    sock_t listenSock, sendSock;

private:
    std::string udp_recv();
    void udp_send(const std::string& data);
};

CPPUNIT_TEST_SUITE_REGISTRATION(ProcessTestSuite);
