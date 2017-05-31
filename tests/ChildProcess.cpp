/**
 * This little program is used to test Rct::Process.
 *
 * It listens to a UDP port and reacts to the following commands:
 *
 * - stdout <string> : Write <string> to stdout
 * - stderr <string> : Write <string> to stderr
 * - exit <retcode>  : Exit, returning <retcode>
 * - getCwd          : Write the current directory to stdout, followed by
 *                     a single \n.
 * - getEnv          : Write the environment to stdout. Format below.
 * - getArgv         : Write the command line (argv array) to stdout.
 *                     Format below.
 *
 * Malformed udp packages are ignored.
 *
 * On socket errors, this program exits with return code -1.
 *
 * Additionally, everything that is read from stdin will be sent
 * via UDP (with a slight, configurable delay).
 *
 * Output format for getEnv: Because environment variables may contain
 * newlines, the \n character is not suitable to delimit the environment
 * list's entries. Rather, each entry is followed by a single binary zero,
 * followed by a \n char. Additionally, the end of the list is signalled
 * by a single entry containing only a binary zero and a \n char, like so:
 * @code
 * <key1>=<value1>\0\n
 * <key2>=<value2>\0\n
 * \0\n
 * @endcode
 *
 * Output format for getArgv: Each entry is delimited by a single binary
 * zero, followed by a \n character. Additionally, the end of the list is
 * signalled by a single entry containing only a binary zero and a \n char.
 */

#include <stdint.h>

//  C O N F I G U R A T I O N

static const uint16_t listenPort = 1337;
static const uint16_t sendPort   = 1338;
static const int      stdinDelay_ms = 20;

//////////////////////////////////////////////

#include <iostream>
#include <string>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <mutex>
#include <thread>

#include "UdpIncludes.h"

#ifdef _WIN32
#  include <direct.h>  // for _getcwd
#endif

void onRecvUdp(void *buf, ssize_t len, int argc, char *argv[], char **envp);

class StdinReader
{
public:
    void operator()();
    std::string getData();
private:
    std::mutex m_mutex;
    std::string m_data;
};

void StdinReader::operator()()
{
    while(std::cin)
    {
        char c;
        std::cin.get(c);

        std::lock_guard<std::mutex> lock(m_mutex);
        if(std::cin) m_data.push_back(c);
    }
}

std::string StdinReader::getData()
{
    std::string ret;
    std::lock_guard<std::mutex> lock(m_mutex);
    ret.swap(m_data);
    return ret;
}

int main(int argc, char * argv[], char *env[])
{
    int res;

#ifdef _WIN32
    {
        WSADATA unused;
        CHECK_RETURN(WSAStartup(MAKEWORD(2,2), &unused) != 0, "WSAStartup()");
    }
#endif

    sock_t listenSock = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK_RETURN(listenSock == INVALID_SOCKET, "socket()");
    sock_t sendSock = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK_RETURN(sendSock == INVALID_SOCKET, "socket() sendSock");

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

    StdinReader in;
    std::thread inThread(std::ref(in));

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
#ifdef _WIN32
            if(WSAGetLastError() == WSAETIMEDOUT)
#else
            if(errno == EAGAIN || errno == EWOULDBLOCK)
#endif
            {
                // We didn't receive anything within the timeout, but that's
                // not an error.
            }
            else
            {
                // error.
                CHECK_RETURN(true, "recvfrom");
                exit(-1);
            }
        }

        if(recvSize > 0)
        {
            onRecvUdp(buf, recvSize, argc, argv, env);
        }

        std::string fromStdin = in.getData();
        recvSize = fromStdin.size();
        if(recvSize > 0)
        {
            res = sendto(sendSock, fromStdin.data(), recvSize, 0,
                         (sockaddr*)&sendAddr, sizeof(sendAddr));

            CHECK_RETURN(res != recvSize, "sendto()");
        }
    }
}

void onRecvUdp(void *f_buf, ssize_t len, int argc, char *argv[], char **envp)
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
        std::cout << buf.substr(7, std::string::npos) << std::flush;
        return;
    }
    else if(buf.size() >= 8 && buf.substr(0,7) == "stderr ")
    {
        std::cerr << buf.substr(7, std::string::npos) << std::flush;
        return;
    }
    else if(buf.size() >= 6 && buf.substr(0,6) == "getCwd")
    {
        /// FIXME this is actually wrong.
        /// See http://tinyurl.com/n6kp7fe (archived: http://archive.is/HgFr4)
        char buffer[PATH_MAX];
#ifdef _WIN32
        char *pwd = _getcwd(buffer, sizeof(buffer));
#else
        char *pwd = getcwd(buffer, sizeof(buffer));
#endif
        std::cout << (pwd ? pwd : "Can't get pwd") << std::endl;
    }
    else if(buf.size() >= 6 && buf.substr(0, 6) == "getEnv")
    {
        while(*envp)
        {
            std::cout << *envp << '\0' << '\n';
            envp++;
        }
        std::cout << '\0' << std::endl;
    }
    else if(buf.size() >= 7 && buf.substr(0,7) == "getArgv")
    {
        for(int i=0; i<argc; i++)
        {
            std::cout << argv[i] << '\0' << '\n';
        }
        std::cout << '\0' << std::endl;
    }
}
