#include "SocketServer.h"
#include "EventLoop.h"
#include "Rct.h"
#include <rct-config.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>

#include "Log.h"

SocketServer::SocketServer()
    : fd(-1), isIPv6(false)
{}

SocketServer::~SocketServer()
{
    close();
}

void SocketServer::close()
{
    if (fd == -1)
        return;
    if (EventLoop::SharedPtr loop = EventLoop::eventLoop())
        loop->unregisterSocket(fd);
    ::close(fd);
    fd = -1;
    if (!path.isEmpty()) {
        Path::rm(path);
        path.clear();
    }
}

bool SocketServer::listen(uint16_t port, Mode mode)
{
    close();

    isIPv6 = (mode & IPv6);

    fd = ::socket(isIPv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        // bad
        serverError(this, InitializeError);
        return false;
    }

    int e;
    int flags = 1;
#ifdef HAVE_NOSIGPIPE
    e = ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&flags, sizeof(int));
    if (e == -1) {
        serverError(this, InitializeError);
        close();
        return false;
    }
#endif
    // turn on nodelay
    flags = 1;
    e = ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(int));
    if (e == -1) {
        serverError(this, InitializeError);
        close();
        return false;
    }
#ifdef HAVE_CLOEXEC
    SocketClient::setFlags(fd, FD_CLOEXEC, F_GETFD, F_SETFD);
#endif

    // ### support specific interfaces
    sockaddr_in addr4;
    sockaddr_in6 addr6;
    sockaddr* addr = 0;
    size_t size = 0;
    if (isIPv6) {
        addr = reinterpret_cast<sockaddr*>(&addr6);
        size = sizeof(sockaddr_in6);
        memset(&addr6, '\0', sizeof(sockaddr_in6));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_addr = in6addr_any;
        addr6.sin6_port = htons(port);
    } else {
        addr = reinterpret_cast<sockaddr*>(&addr4);
        size = sizeof(sockaddr_in);
        memset(&addr4, '\0', sizeof(sockaddr_in));
        addr4.sin_family = AF_INET;
        addr4.sin_addr.s_addr = INADDR_ANY;
        addr4.sin_port = htons(port);
    }

    return commonBindAndListen(addr, size);
}

bool SocketServer::listen(const Path &p)
{
    close();

    fd = ::socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        // bad
        serverError(this, InitializeError);
        return false;
    }
#ifdef HAVE_CLOEXEC
    SocketClient::setFlags(fd, FD_CLOEXEC, F_GETFD, F_SETFD);
#endif

    sockaddr_un addr;
    memset(&addr, '\0', sizeof(sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, p.constData(), sizeof(addr.sun_path));

    if (commonBindAndListen(reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_un))) {
        path = p;
        return true;
    }
    return false;
}

bool SocketServer::listenfd(int fdArg)
{
    close();

    fd = fdArg;

    return commonListen();
}

bool SocketServer::commonBindAndListen(sockaddr* addr, size_t size)
{
    if (::bind(fd, reinterpret_cast<sockaddr*>(addr), size) < 0) {
        serverError(this, BindError);
        close();
        return false;
    }

    return commonListen();
}

bool SocketServer::commonListen()
{
    // ### should be able to customize the backlog
    enum { Backlog = 128 };
    if (::listen(fd, Backlog) < 0) {
        fprintf(stderr, "::listen() failed with errno: %s\n",
                Rct::strerror().constData());

        serverError(this, ListenError);
        close();
        return false;
    }

    if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
        loop->registerSocket(fd, EventLoop::SocketRead,
                             //|EventLoop::SocketWrite,
                             std::bind(&SocketServer::socketCallback,
                                       this,
                                       std::placeholders::_1,
                                       std::placeholders::_2));
        if (!SocketClient::setFlags(fd, O_NONBLOCK, F_GETFL, F_SETFL)) {
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
    return SocketClient::SharedPtr(new SocketClient(fd, path.isEmpty() ? SocketClient::Tcp : SocketClient::Unix));
}

void SocketServer::socketCallback(int /*fd*/, int mode)
{
    sockaddr_in client4;
    sockaddr_in6 client6;
    sockaddr* client = 0;
    socklen_t size = 0;
    if (isIPv6) {
        size = sizeof(client6);
        client = reinterpret_cast<sockaddr*>(&client6);
    } else {
        size = sizeof(client4);
        client = reinterpret_cast<sockaddr*>(&client4);
    }
    int e;

    if (! ( mode & EventLoop::SocketRead ) )
        return;

    for (;;) {
        eintrwrap(e, ::accept(fd, client, &size));
        if (e == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            serverError(this, AcceptError);
            close();
            return;
        }

        //EventLoop::eventLoop()->unregisterSocket( fd );
        accepted.push(e);
        serverNewConnection(this);
    }
}
