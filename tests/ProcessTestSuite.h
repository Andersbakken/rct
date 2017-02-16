#include <cppunit/extensions/HelperMacros.h>

#include "UdpIncludes.h"

class ProcessTestSuite : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(ProcessTestSuite);
    CPPUNIT_TEST(returnCode);
    CPPUNIT_TEST(startAsync);
    CPPUNIT_TEST(readFromStdout);
    CPPUNIT_TEST(readFromStderr);
    CPPUNIT_TEST(signals);
    CPPUNIT_TEST(execTimeout);
    CPPUNIT_TEST(writeToStdin);
    CPPUNIT_TEST(startNonExistingProgram);
    CPPUNIT_TEST(startUnicodeProgram);
    CPPUNIT_TEST(commandLineArgs);
#ifdef _WIN32
    CPPUNIT_TEST(killWindows);
    CPPUNIT_TEST(destructorWindows);
#endif
    CPPUNIT_TEST(env);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp() override;     ///< Set up the required udp sockets.
    void tearDown() override;  ///< Close the sockets.

    static const int recvTimeout_ms = 100;
    static const uint16_t listenPort = 1338; ///< Must be ChildProcess' sendPort
    static const uint16_t sendPort   = 1337; ///< Must be ChildProcess' listenPort

protected:
    /// start a process and examine its return code
    void returnCode();

    /// start a process asynchronously
    void startAsync();

    /// read data from stdout while the child process is running
    void readFromStdout();

    /// read data from stderr while the child process is running
    void readFromStderr();

    /// test whether Process sends the correct signals through EventLoop
    /// when the child process writes stuff to stdout, stderr or when it
    /// exits.
    void signals();

    /// Start a process that does not terminate in the requested time.
    /// Process::exec() should return after the timeout.
    void execTimeout();

    /// Write some data to ChildProcess' stdin and check if the child
    /// receives the data.
    void writeToStdin();

    /// Try to start a program which does not exist.
    void startNonExistingProgram();

    /// Try to start a program whose name has some non-English characters.
    void startUnicodeProgram();

    // Add some command line arguments
    void commandLineArgs();

#ifdef _WIN32
    // test "kill" on windows -- it can only terminate the child.
    void killWindows();

    // delete a Process object when the associated child process is still
    // running -- only works on windows.
    void destructorWindows();
#endif

    /// Test whether setting the environment for the child process works.
    void env();

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
