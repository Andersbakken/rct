#include "ProcessTestSuite.h"

#include <rct/Process.h>
#include <thread>
#include <mutex>
#include "UdpIncludes.h"

sock_t listenSock, sendSock;

void ProcessTestSuite::setUp()
{
    int res;

    listenSock = socket(AF_INET, SOCK_DGRAM, 0);
    sendSock = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK_RETURN(listenSock < 0 || sendSock < 0, "socket()");

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

    sockaddr_in listenAddr;
    std::memset(&listenAddr, 0, sizeof(listenAddr));
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); //only bind localhost
    listenAddr.sin_port = htons(1338);
    res = bind(listenSock, (sockaddr*)&listenAddr, sizeof(listenAddr));
    CHECK_RETURN(res < 0, "bind");
}

static std::string recv()
{
    static const size_t BUFFER_SIZE = 80;
    char buf[BUFFER_SIZE];
    ssize_t recvSize = recv(listenSock, buf, BUFFER_SIZE, 0);

    if(recvSize < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        std::cout << "Error reading from udp socket" << std::endl;
        exit(-1);
    }

    if(recvSize < 0) recvSize = 0;
    std::string ret(buf, recvSize);
    return ret;
}

static void send(const std::string &data)
{
    sockaddr_in sendAddr;
    std::memset(&sendAddr, 0, sizeof(sendAddr));
    sendAddr.sin_family = AF_INET;
    sendAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sendAddr.sin_port = htons(1337);

    int res = sendto(sendSock, data.data(), data.size(), 0,
                     (sockaddr*)&sendAddr, sizeof(sendAddr));

    CHECK_RETURN((unsigned)res != data.size(), "Error sending UDP");
}

void ProcessTestSuite::tearDown()
{
    close(listenSock);
    close(sendSock);
}

void ProcessTestSuite::returnCode()
{
    // tell child process to exit with code 12 in 1 second from now
    std::thread t([](){sleep(1); send("exit 12");});

    // start process
    Process p;
    p.exec("ChildProcess");

    t.join();

    // check exit code
    CPPUNIT_ASSERT(p.returnCode() == 12);
}
