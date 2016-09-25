/**
 * This little program is used to test Rct::Process.
 *
 * It listens to a UDP port and reacts to the following commands:
 *
 * - stdout <string> : Write <string> to stdout
 * - stderr <string> : Write <string> to stderr
 * - exit <retcode>  : Exit, returning <retcode>
 * - getCwd          : Write the current directory to stdout.
 * - getEnv          : Write the environment to stdout.
 *
 * Malformed udp packages are ignored.
 *
 * On socket errors, this program exits with return code -1.
 *
 * Additionally, everything that is read from stdin will be sent
 * via UDP (with a slight, configurable delay).
 */

#include <stdint.h>

//  C O N F I G U R A T I O N

static const uint16_t listenPort = 1337;
static const uint16_t sendPort   = 1338;
static const int      stdinDelay_ms = 1000;

//////////////////////////////////////////////

#include <iostream>
#include <string>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <sstream>

#ifdef _WIN32
#  include <Windows.h>
#  include <Winsock2.h>
   typedef SOCKET sock_t;
   typedef const char *optval_p;
   typedef DWORD timeout_t;
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/ip.h>
   typedef int sock_t;
   typedef const void *optval_p;
   typedef struct timeval timeout_t;
#endif



#define CHECK_RETURN(errorCondition, msg)               \
    if(errorCondition)                                  \
    {                                                   \
        std::cout << "Error: " << (msg)                 \
                  << " in line " << __LINE__            \
                  << ", errno="  << errno << std::endl; \
    }

void onRecvUdp(void *buf, ssize_t len);

int main()
{
    int res;
    sock_t listenSock = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK_RETURN(listenSock < 0, "socket()");
    sock_t sendSock = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK_RETURN(sendSock < 0, "socket() sendSock");

    // allow the server to bind again immediately after killing it.
    const int optval = 1;
    res = setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (optval_p)&optval,
                     sizeof(int));
    CHECK_RETURN(res != 0, "setsockopt(SO_REUSEADDR)");

    // set timeout on reads
#ifdef _WIN32
    timeout_t to = stdinDelay_ms;
#else
    timeout_t to;
    to.tv_sec = stdinDelay_ms / 1000;
    to.tv_usec = (stdinDelay_ms % 1000) * 1000;
#endif
    res = setsockopt(listenSock, SOL_SOCKET, SO_RCVTIMEO, (optval_p)&to,
                     sizeof(timeout_t));
    CHECK_RETURN(res != 0, "setsockopt(SO_RCVTIMEO)");

    struct sockaddr_in listenAddr;
    std::memset(&listenAddr, 0, sizeof(listenAddr));
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    listenAddr.sin_port = htons(listenPort);
    res = bind(listenSock, (sockaddr*)&listenAddr, sizeof(listenAddr));
    CHECK_RETURN(res < 0, "bind");

    sockaddr_in sendAddr;
    std::memset(&sendAddr, 0, sizeof(sendAddr));
    sendAddr.sin_family = AF_INET;
    sendAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sendAddr.sin_port = htons(sendPort);

    for(;;)   // endless loop
    {
        sockaddr_in clientAddr;
        socklen_t clientAddrSize = sizeof(clientAddr);
        static const size_t BUFFER_SIZE = 80;
        char buf[BUFFER_SIZE];
        ssize_t recvSize = recvfrom(listenSock, buf, BUFFER_SIZE, 0,
                                    (sockaddr*)&clientAddr, &clientAddrSize);

        if(recvSize < 0)
        {
            //maybe an error?
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // We didn't receive anything within the timeout, but that's
                // not an error.
            }
            else
            {
                // error.
                exit(-1);
            }
        }

        if(recvSize > 0)
        {
            onRecvUdp(buf, recvSize);
        }

        // send stdin data via udp (reuse buf)
        // TODO: Doesn't work as expected. Need an extra thread for this.
        recvSize = std::cin.readsome(buf, BUFFER_SIZE);

        if(recvSize > 0)
        {
            std::cout << "read " << recvSize << " from stdin\n";
            res = sendto(sendSock, buf, recvSize, 0, (sockaddr*)&sendAddr,
                         sizeof(sendAddr));

            CHECK_RETURN(res != recvSize, "sendto()");
        }
    }
}

void onRecvUdp(void *f_buf, ssize_t len)
{
    std::string buf((char*)f_buf, len);

    if(buf.size() >= 6 && buf.substr(0, 5) == "exit ")
    {
        int exitCode;
        std::istringstream iss(buf.substr(5, std::string::npos));
        if(!(iss >> exitCode)) return; //can't read number
        exit(exitCode);
    }
    else if(buf.size() >= 8 && buf.substr(0, 7) == "stdout ")
    {
        std::cout << buf.substr(7, std::string::npos);
        return;
    }
    else if(buf.size() >= 8 && buf.substr(0,7) == "stderr")
    {
        std::cerr << buf.substr(7, std::string::npos);
        return;
    }
}
