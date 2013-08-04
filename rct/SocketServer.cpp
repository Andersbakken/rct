#include "SocketServer.h"
#include "EventLoop.h"
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>

#define eintrwrap(VAR, BLOCK)                   \
    do {                                        \
        VAR = BLOCK;                            \
    } while (VAR == -1 && errno == EINTR)

SocketServer::SocketServer()
    : fd(-1)
{
}

SocketServer::~SocketServer()
{
    close();
}

void SocketServer::close()
{
    if (fd == -1)
        return;
    if (EventLoop::SharedPtr loop = EventLoop::mainEventLoop())
        loop->unregisterSocket(fd);
    ::close(fd);
    fd = -1;
}

bool SocketServer::listen(uint16_t port)
{
    close();

    fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        // bad
        serverError(this, InitializeError);
        return false;
    }

    // turn on nodelay
    int e;
    {
        int on = 1;
        e = ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        if (e == -1) {
            serverError(this, InitializeError);
            close();
            return false;
        }
    }

    // ### support IPv6 and specific interfaces
    sockaddr_in addr;
    memset(&addr, '\0', sizeof(sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    return commonListen(reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
}

bool SocketServer::listen(const std::string& path)
{
    close();

    fd = ::socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        // bad
        serverError(this, InitializeError);
        return false;
    }

    sockaddr_un addr;
    memset(&addr, '\0', sizeof(sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path));

    return commonListen(reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_un));
}

bool SocketServer::commonListen(sockaddr* addr, size_t size)
{
    if (::bind(fd, reinterpret_cast<sockaddr*>(addr), size) < 0) {
        serverError(this, BindError);
        close();
        return false;
    }

    // ### should be able to customize the backlog
    if (::listen(fd, 5) < 0) {
        serverError(this, ListenError);
        close();
        return false;
    }

    if (EventLoop::SharedPtr loop = EventLoop::mainEventLoop()) {
        loop->registerSocket(fd, EventLoop::SocketRead|EventLoop::SocketWrite,
                             std::bind(&SocketServer::socketCallback, this, std::placeholders::_1, std::placeholders::_2));
        int e;
        eintrwrap(e, ::fcntl(fd, F_GETFL, 0));
        if (e != -1) {
            eintrwrap(e, ::fcntl(fd, F_SETFL, e | O_NONBLOCK));
        } else {
            serverError(this, InitializeError);
            close();
            return false;
        }
    }

    return true;
}

SocketClient::SharedPtr SocketServer::nextConnection()
{
    if (accepted.empty())
        return 0;
    const int fd = accepted.front();
    accepted.pop();
    return SocketClient::SharedPtr(new SocketClient(fd));
}

void SocketServer::socketCallback(int /*fd*/, int /*mode*/)
{
    sockaddr_in client;
    socklen_t size = sizeof(sockaddr_in);
    int e;
    for (;;) {
        eintrwrap(e, ::accept(fd, reinterpret_cast<sockaddr*>(&client), &size));
        if (e == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            serverError(this, AcceptError);
            close();
            return;
        }
        accepted.push(e);
        serverNewConnection(this);
    }
}
