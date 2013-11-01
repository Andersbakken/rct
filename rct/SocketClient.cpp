#include "SocketClient.h"
#include "EventLoop.h"
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <rct-config.h>

#define eintrwrap(VAR, BLOCK)                   \
    do {                                        \
        VAR = BLOCK;                            \
    } while (VAR == -1 && errno == EINTR)

SocketClient::SocketClient(Mode mode)
    : socketState(Disconnected), socketMode(mode), wMode(Asynchronous), writeWait(false)
{
    int domain = -1, type = -1;
    switch (mode) {
    case Udp:
        type = SOCK_DGRAM;
        domain = AF_INET;
        break;
    case Tcp:
        type = SOCK_STREAM;
        domain = AF_INET;
        break;
    case Unix:
        type = SOCK_STREAM;
        domain = PF_UNIX;
        break;
    }

    fd = ::socket(domain, type, 0);
    if (fd < 0) {
        // bad
        signalError(shared_from_this(), InitializeError);
        return;
    }
#ifdef HAVE_NOSIGPIPE
    int flags = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&flags, sizeof(int));
#endif
    if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
        loop->registerSocket(fd, EventLoop::SocketRead,
                             std::bind(&SocketClient::socketCallback, this, std::placeholders::_1, std::placeholders::_2));
        int e;
        eintrwrap(e, ::fcntl(fd, F_GETFL, 0));
        if (e != -1) {
            eintrwrap(e, ::fcntl(fd, F_SETFL, e | O_NONBLOCK));
        } else {
            signalError(shared_from_this(), InitializeError);
            close();
            return;
        }
    }
}

SocketClient::SocketClient(int f, Mode mode)
    : fd(f), socketState(Connected), socketMode(mode), writeWait(false)
{
    assert(fd >= 0);
#ifdef HAVE_NOSIGPIPE
    int flags = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&flags, sizeof(int));
#endif

    if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
        loop->registerSocket(fd, EventLoop::SocketRead,
                             std::bind(&SocketClient::socketCallback, this, std::placeholders::_1, std::placeholders::_2));
        int e;
        eintrwrap(e, ::fcntl(fd, F_GETFL, 0));
        if (e != -1) {
            eintrwrap(e, ::fcntl(fd, F_SETFL, e | O_NONBLOCK));
        } else {
            signalError(shared_from_this(), InitializeError);
            close();
            return;
        }
    }
}

SocketClient::~SocketClient()
{
    close();
}

void SocketClient::close()
{
    if (fd == -1)
        return;
    socketState = Disconnected;
    if (EventLoop::SharedPtr loop = EventLoop::eventLoop())
        loop->unregisterSocket(fd);
    ::close(fd);
    fd = -1;
}

class Resolver
{
public:
    Resolver();
    Resolver(const std::string& host, uint16_t port, const SocketClient::SharedPtr& socket);
    ~Resolver();

    void resolve(const std::string& host, uint16_t port, const SocketClient::SharedPtr& socket);

    addrinfo* res;
    sockaddr* addr;
    size_t size;
};

Resolver::Resolver()
    : res(0), addr(0), size(0)
{
}

Resolver::Resolver(const std::string& host, uint16_t port, const SocketClient::SharedPtr& socket)
    : res(0), addr(0), size(0)
{
    resolve(host, port, socket);
}

void Resolver::resolve(const std::string& host, uint16_t port, const SocketClient::SharedPtr& socket)
{
    // first, see if this parses as an IPv4 address
    {
        struct in_addr inaddr;
        if (inet_aton(host.c_str(), &inaddr) == 1) {
            // yes, use that
            sockaddr_in* newaddr = new sockaddr_in;
            memset(newaddr, '\0', sizeof(sockaddr_in));
            memcpy(&newaddr->sin_addr, &inaddr, sizeof(struct in_addr));
            newaddr->sin_family = AF_INET;
            newaddr->sin_port = htons(port);
            addr = reinterpret_cast<sockaddr*>(newaddr);
            size = sizeof(sockaddr_in);
            return;
        }
    }

    // not an ip address, try to resolve it
    addrinfo hints, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.c_str(), NULL, &hints, &res) != 0) {
        // bad
        socket->signalError(socket, SocketClient::DnsError);
        socket->close();
        return;
    }

    for (p = res; p; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            addr = p->ai_addr;
            reinterpret_cast<sockaddr_in*>(addr)->sin_port = htons(port);
            size = sizeof(sockaddr_in);
            break;
        } else if (p->ai_family == AF_INET6) {
            addr = p->ai_addr;
            reinterpret_cast<sockaddr_in6*>(addr)->sin6_port = htons(port);
            size = sizeof(sockaddr_in6);
            break;
        }
    }

    if (!addr) {
        socket->signalError(socket, SocketClient::DnsError);
        socket->close();
        freeaddrinfo(res);
        res = 0;
        return;
    }
}

Resolver::~Resolver()
{
    if (res)
        freeaddrinfo(res); // free the linked list
    else if (addr)
        delete reinterpret_cast<sockaddr_in*>(addr);
}

bool SocketClient::connect(const std::string& host, uint16_t port)
{
    SocketClient::SharedPtr tcpSocket = shared_from_this();
    Resolver resolver(host, port, tcpSocket);
    if (!resolver.addr)
        return false;

    int e;
    eintrwrap(e, ::connect(fd, resolver.addr, resolver.size));
    if (e == 0) { // we're done
        socketState = Connected;

        signalConnected(tcpSocket);
    } else {
        if (errno != EINPROGRESS) {
            // bad
            signalError(tcpSocket, ConnectError);
            close();
            return false;
        }
        if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
            loop->updateSocket(fd, EventLoop::SocketRead|EventLoop::SocketWrite|EventLoop::SocketOneShot);
            writeWait = true;
        }
        socketState = Connecting;
    }

    return true;
}

bool SocketClient::connect(const std::string& path)
{
    sockaddr_un addr;
    memset(&addr, '\0', sizeof(sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path));

    SocketClient::SharedPtr unixSocket = shared_from_this();

    int e;
    eintrwrap(e, ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_un)));
    if (e == 0) { // we're done
        socketState = Connected;

        signalConnected(unixSocket);
    } else {
        if (errno != EINPROGRESS) {
            // bad
            signalError(unixSocket, ConnectError);
            close();
            return false;
        }
        if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
            loop->updateSocket(fd, EventLoop::SocketRead|EventLoop::SocketWrite|EventLoop::SocketOneShot);
            writeWait = true;
        }
        socketState = Connecting;
    }
    return true;
}

bool SocketClient::bind(uint16_t port)
{
    sockaddr_in addr;
    memset(&addr, '\0', sizeof(sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    const int e = ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in));
    if (!e)
        return true;

    SocketClient::SharedPtr udpSocket = shared_from_this();
    signalError(udpSocket, BindError);
    close();
    return false;
}

bool SocketClient::addMembership(const std::string& ip)
{
    struct ip_mreq mreq;
    if (inet_aton(ip.c_str(), &mreq.imr_multiaddr) == 0)
        return false;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    ::setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    return true;
}

bool SocketClient::dropMembership(const std::string& ip)
{
    struct ip_mreq mreq;
    if (inet_aton(ip.c_str(), &mreq.imr_multiaddr) == 0)
        return false;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    ::setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
    return true;
}

void SocketClient::setMulticastLoop(bool loop)
{
    const unsigned char ena = loop ? 1 : 0;
    ::setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &ena, 1);
}

bool SocketClient::peer(std::string* ip, uint16_t* port)
{
    if (socketMode == Unix)
        return false;
    sockaddr_in addr;
    socklen_t size = sizeof(addr);
    if (::getpeername(fd, reinterpret_cast<sockaddr*>(&addr), &size) == -1)
        return false;
    if (ip) {
        ip->resize(46);
        inet_ntop(AF_INET, &addr.sin_addr, &(*ip)[0], ip->size());
        ip->resize(strlen(ip->c_str()));
    }
    if (port)
        *port = ntohs(addr.sin_port);
    return true;
}

bool SocketClient::writeTo(const std::string& host, uint16_t port, const unsigned char* data, unsigned int size)
{
    SocketClient::SharedPtr socketPtr = shared_from_this();

    Resolver resolver;
    if (port != 0)
        resolver.resolve(host, port, socketPtr);

    int e;
    bool done = !data;
    unsigned int total = 0;

#ifdef HAVE_NOSIGNAL
    const int sendFlags = MSG_NOSIGNAL;
#else
    const int sendFlags = 0;
#endif

    if (!writeWait) {
        if (!writeBuffer.isEmpty()) {
            while (total < writeBuffer.size()) {
                assert(writeBuffer.size() > total);
                if (resolver.addr)
                    eintrwrap(e, ::sendto(fd, writeBuffer.data() + total, writeBuffer.size() - total,
                                          sendFlags, resolver.addr, resolver.size));
                else
                    eintrwrap(e, ::write(fd, writeBuffer.data() + total, writeBuffer.size() - total));
                if (e == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        if (wMode == Synchronous) {
                            if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
                                if (total) {
                                    if (total < writeBuffer.size()) {
                                        memmove(writeBuffer.data(), writeBuffer.data() + total, writeBuffer.size() - total);
                                    }
                                    writeBuffer.resize(writeBuffer.size() - total);
                                    total = 0;
                                }
                                if (loop->processSocket(fd) & EventLoop::SocketWrite)
                                    break;
                                if (fd == -1)
                                    return false;
                            }
                        }
                        assert(!writeWait);
                        if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
                            loop->updateSocket(fd, EventLoop::SocketRead|EventLoop::SocketWrite|EventLoop::SocketOneShot);
                            writeWait = true;
                        }
                        done = true;
                        break;
                    } else {
                        // bad
                        signalError(shared_from_this(), WriteError);
                        close();
                        return false;
                    }
                }
                signalBytesWritten(socketPtr, e);
                total += e;
            }
            if (total) {
                if (total < writeBuffer.size()) {
                    memmove(writeBuffer.data(), writeBuffer.data() + total, writeBuffer.size() - total);
                }
                writeBuffer.resize(writeBuffer.size() - total);
            }
        }

        if (done || fd == -1) {
            return fd != -1;
        }
        total = 0;

        assert(data != 0 && size > 0);

        for (;;) {
            assert(size > total);
            if (resolver.addr)
                eintrwrap(e, ::sendto(fd, data + total, size - total,
                                      sendFlags, resolver.addr, resolver.size));
            else
                eintrwrap(e, ::write(fd, data + total, size - total));
            if (e == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (wMode == Synchronous) {
                        if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
                            // store the rest
                            const unsigned rem = size - total;
                            writeBuffer.reserve(writeBuffer.size() + rem);
                            memcpy(writeBuffer.end(), data + total, rem);
                            writeBuffer.resize(writeBuffer.size() + rem);

                            (void)loop->processSocket(fd);
                            return isConnected();
                        }
                    }
                    assert(!writeWait);
                    if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
                        loop->updateSocket(fd, EventLoop::SocketRead|EventLoop::SocketWrite|EventLoop::SocketOneShot);
                        writeWait = true;
                    }
                    break;
                } else {
                    // bad
                    signalError(shared_from_this(), WriteError);
                    close();
                    return false;
                }
            }
            signalBytesWritten(socketPtr, e);
            total += e;
            assert(total <= size);
            if (total == size) {
                // we're done
                return true;
            }
        }
    }

    if (total < size) {
        // store the rest
        const unsigned rem = size - total;
        writeBuffer.reserve(writeBuffer.size() + rem);
        memcpy(writeBuffer.end(), data + total, rem);
        writeBuffer.resize(writeBuffer.size() + rem);
    }
    return true;
}

bool SocketClient::write(const unsigned char* data, unsigned int size)
{
    return writeTo(std::string(), 0, data, size);
}

static std::string addrToString(const sockaddr_in& addr)
{
    return inet_ntoa(addr.sin_addr);
}

static uint16_t addrToPort(const sockaddr_in& addr)
{
    return ntohs(addr.sin_port);
}

void SocketClient::socketCallback(int f, int mode)
{
    assert(f == fd);
    (void)f;

    SocketClient::SharedPtr socketPtr = shared_from_this();

    if (mode == EventLoop::SocketError) {
        signalError(socketPtr, EventLoopError);
        close();
        return;
    }

    if (writeWait && (mode & EventLoop::SocketWrite)) {

        if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
            loop->updateSocket(fd, EventLoop::SocketRead);
            writeWait = false;
        }
    }

    sockaddr_in fromAddr;
    socklen_t fromLen = 0;

    if (mode & EventLoop::SocketRead) {

        enum { BlockSize = 1024, AllocateAt = 512 };
        int e;

        unsigned int total = 0;
        for(;;) {
            unsigned int rem = readBuffer.capacity() - readBuffer.size();
            // printf("reading, remaining size %u\n", rem);
            if (rem <= AllocateAt) {
                // printf("allocating more\n");
                readBuffer.reserve(readBuffer.size() + BlockSize);
                rem = readBuffer.capacity() - readBuffer.size();
                // printf("Rem is now %d\n", rem);
            }
            if (socketMode == Udp) {
                fromLen = sizeof(fromAddr);
                eintrwrap(e, ::recvfrom(fd, readBuffer.end(), rem, 0, reinterpret_cast<sockaddr*>(&fromAddr), &fromLen));
            } else {
                eintrwrap(e, ::read(fd, readBuffer.end(), rem));
            }
            if (e == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    // bad
                    signalError(socketPtr, ReadError);
                    close();
                    return;
                }
            } else if (e == 0) {
                // socket closed
                if (total) {
                    if (!fromLen) {
                        signalReadyRead(socketPtr, std::move(readBuffer));
                    } else {
                        signalReadyReadFrom(socketPtr, addrToString(fromAddr), addrToPort(fromAddr), std::move(readBuffer));
                        readBuffer.clear();
                    }
                }
                signalDisconnected(socketPtr);
                close();
                return;
            } else {
                total += e;
                readBuffer.resize(total);
            }
        }
        assert(total <= readBuffer.capacity());
        if (!fromLen)
            signalReadyRead(socketPtr, std::move(readBuffer));
        else
            signalReadyReadFrom(socketPtr, addrToString(fromAddr), addrToPort(fromAddr), std::move(readBuffer));

        if (writeWait) {
            if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
                loop->updateSocket(fd, EventLoop::SocketRead|EventLoop::SocketWrite|EventLoop::SocketOneShot);
            }
        }
    }
    if (mode & EventLoop::SocketWrite) {
        if (socketState == Connecting) {
            int err;
            socklen_t size = sizeof(err);
            int e = ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &size);
            if (e == -1) {
                // bad
                signalError(socketPtr, ConnectError);
                close();
                return;
            }
            if (!err) {
                // connected
                socketState = Connected;
                signalConnected(socketPtr);
            } else {
                // failed to connect
                signalError(socketPtr, ConnectError);
                close();
                return;
            }
        }
        write(0, 0);
    }
}
