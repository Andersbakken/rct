#include "SocketClient.h"

#ifdef _WIN32
#  include <Winsock2.h>
#  include <Ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <sys/un.h>
#endif
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "EventLoop.h"
#include "Log.h"
#include "rct/rct-config.h"
#include "Rct.h"

#ifdef NDEBUG
struct Null { template <typename T> Null operator<<(const T &) { return *this; } };
#define DEBUG() if (false) Null()
#else
#define DEBUG() if (mLogsEnabled) debug()
#endif

SocketClient::SocketClient(unsigned int mode)
    : fd(-1), socketPort(0), socketState(Disconnected), socketMode(None),
      wMode(Asynchronous), writeWait(false), mLogsEnabled(true), writeOffset(0)
{
    blocking = (mode & Blocking);
}

SocketClient::SocketClient(int f, unsigned int mode)
    : fd(f), socketPort(0), socketState(Connected), socketMode(mode),
      wMode(Asynchronous), writeWait(false), mLogsEnabled(true), writeOffset(0)
{
    assert(fd >= 0);
#ifdef HAVE_NOSIGPIPE
    int flags = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&flags, sizeof(int));
#endif
#ifdef HAVE_CLOEXEC
    setFlags(fd, FD_CLOEXEC, F_GETFD, F_SETFD);
#endif
    blocking = (mode & Blocking);

    if (!blocking) {
        if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
            loop->registerSocket(fd, EventLoop::SocketRead,
                                 std::bind(&SocketClient::socketCallback, this, std::placeholders::_1, std::placeholders::_2));
#ifndef _WIN32
            if (!setFlags(fd, O_NONBLOCK, F_GETFL, F_SETFL)) {
                signalError(shared_from_this(), InitializeError);
                close();
                return;
            }
#endif
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
    if (!blocking) {
        if (EventLoop::SharedPtr loop = EventLoop::eventLoop())
            loop->unregisterSocket(fd);
    }
    ::close(fd);
    socketPort = 0;
    address.clear();
    fd = -1;
}

class Resolver
{
public:
    Resolver();
    Resolver(const String& host, uint16_t port, const SocketClient::SharedPtr& socket);
    ~Resolver();

    void resolve(const String& host, uint16_t port, const SocketClient::SharedPtr& socket);

    addrinfo* res;
    sockaddr* addr;
    size_t size;
};

Resolver::Resolver()
    : res(0), addr(0), size(0)
{
}

Resolver::Resolver(const String& host, uint16_t port, const SocketClient::SharedPtr& socket)
    : res(0), addr(0), size(0)
{
    resolve(host, port, socket);
}

void Resolver::resolve(const String& host, uint16_t port, const SocketClient::SharedPtr& socket)
{
    // first, see if this parses as an IP address
    {
        struct in_addr inaddr4;
        struct in6_addr inaddr6;
        struct { int af; void* dst; } addrs[2] = {
            { AF_INET, &inaddr4 },
            { AF_INET6, &inaddr6 }
        };
        for (int i = 0; i < 2; ++i) {
            if (inet_pton(addrs[i].af, host.constData(), addrs[i].dst) == 1) {
                // yes, use that
                if (addrs[i].af == AF_INET) {
                    sockaddr_in* newaddr = new sockaddr_in;
                    memset(newaddr, '\0', sizeof(sockaddr_in));
                    memcpy(&newaddr->sin_addr, &inaddr4, sizeof(struct in_addr));
                    newaddr->sin_family = AF_INET;
                    newaddr->sin_port = htons(port);
                    addr = reinterpret_cast<sockaddr*>(newaddr);
                    size = sizeof(sockaddr_in);
                } else {
                    assert(addrs[i].af == AF_INET6);
                    sockaddr_in6* newaddr = new sockaddr_in6;
                    memset(newaddr, '\0', sizeof(sockaddr_in6));
                    memcpy(&newaddr->sin6_addr, &inaddr6, sizeof(struct in6_addr));
                    newaddr->sin6_family = AF_INET6;
                    newaddr->sin6_port = htons(port);
                    addr = reinterpret_cast<sockaddr*>(newaddr);
                    size = sizeof(sockaddr_in6);
                }
                return;
            }
        }
    }

    // not an ip address, try to resolve it
    addrinfo hints, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.constData(), NULL, &hints, &res) != 0) {
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
    else if (addr) {
        if (size == sizeof(sockaddr_in))
            delete reinterpret_cast<sockaddr_in*>(addr);
        else
            delete reinterpret_cast<sockaddr_in6*>(addr);
    }
}

bool SocketClient::connect(const String& host, uint16_t port)
{
    SocketClient::SharedPtr tcpSocket = shared_from_this();
    Resolver resolver(host, port, tcpSocket);
    if (!resolver.addr)
        return false;

    unsigned int mode = Tcp;
    if (resolver.size == sizeof(sockaddr_in6))
        mode |= IPv6;

    if (!init(mode))
        return false;

    int e;
    eintrwrap(e, ::connect(fd, resolver.addr, resolver.size));
    socketPort = port;
    address = host;
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

#ifndef _WIN32
bool SocketClient::connect(const String& path)
{
    if (!init(Unix))
        return false;

    union {
        sockaddr_un addr_un;
        sockaddr addr;
    };
    memset(&addr_un, '\0', sizeof(addr_un));
    addr_un.sun_family = AF_UNIX;
    strncpy(addr_un.sun_path, path.constData(), sizeof(addr_un.sun_path));

    SocketClient::SharedPtr unixSocket = shared_from_this();

    int e;
    eintrwrap(e, ::connect(fd, &addr, sizeof(addr_un)));
    address = path;
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
#endif

bool SocketClient::bind(uint16_t port)
{
    if (!init(Udp))
        return false;
    union {
        sockaddr_in addr4;
        sockaddr_in6 addr6;
        sockaddr addr;
    };
    int size = 0;
    if (socketMode & IPv6) {
        size = sizeof(sockaddr_in6);
        memset(&addr6, '\0', size);
        addr6.sin6_family = AF_INET6;
        addr6.sin6_addr = in6addr_any;
        addr6.sin6_port = htons(port);
    } else {
        size = sizeof(sockaddr_in);
        memset(&addr4, '\0', size);
        addr4.sin_family = AF_INET;
        addr4.sin_addr.s_addr = htonl(INADDR_ANY);
        addr4.sin_port = htons(port);
    }

#ifdef _WIN32
    int e;
    bool in = true;
    const char *pin = reinterpret_cast<const char*>(&in);
#else
    int e = 1;
    int in = e;
    int *pin = &in;
#endif
    e = ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, pin, sizeof(in));
    if (e == -1) {
        SocketClient::SharedPtr udpSocket = shared_from_this();
        signalError(udpSocket, BindError);
        close();
        return false;
    }
    e = ::bind(fd, &addr, size);
    if (!e) {
        socketPort = port;
        return true;
    }

    SocketClient::SharedPtr udpSocket = shared_from_this();
    signalError(udpSocket, BindError);
    close();
    return false;
}

bool SocketClient::addMembership(const String& ip)
{
    struct ip_mreq mreq;
    if (inet_pton(AF_INET, ip.constData(), &mreq.imr_multiaddr) == 0)
        return false;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    ::setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq));
    return true;
}

bool SocketClient::dropMembership(const String& ip)
{
    struct ip_mreq mreq;
    if (inet_pton(AF_INET, ip.constData(), &mreq.imr_multiaddr) == 0)
        return false;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    ::setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq));
    return true;
}

void SocketClient::setMulticastLoop(bool loop)
{
    const char ena = loop ? 1 : 0;
    ::setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &ena, sizeof(ena));
}

void SocketClient::setMulticastTTL(unsigned char ttl)
{
    ::setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<char*>(&ttl), sizeof(ttl));
}

#ifdef _WIN32
typedef int (*GetNameFunc)(SOCKET, sockaddr*, socklen_t*);
#else
typedef int (*GetNameFunc)(int, sockaddr*, socklen_t*);
#endif
static inline String getNameHelper(int fd, GetNameFunc func, uint16_t* port)
{
    union {
        sockaddr_storage storage;
        sockaddr_in6 addr6;
        sockaddr_in addr4;
        sockaddr addr;
    };
    socklen_t size = sizeof(storage);
    if (func(fd, &addr, &size) == -1)
        return String();
    String name(INET6_ADDRSTRLEN, '\0');
    if (storage.ss_family == AF_INET6) {
        inet_ntop(AF_INET6, &addr6.sin6_addr, name.data(), name.size());
        name.resize(strlen(name.constData()));
        if (port)
            *port = ntohs(addr6.sin6_port);
    } else {
        assert(storage.ss_family == AF_INET);
        inet_ntop(AF_INET, &addr4.sin_addr, name.data(), name.size());
        name.resize(strlen(name.constData()));
        if (port)
            *port = ntohs(addr4.sin_port);
    }
    return name;
}

String SocketClient::peerName(uint16_t* port) const
{
    if (socketMode & Unix)
        return String();
    return getNameHelper(fd, ::getpeername, port);
}

String SocketClient::sockName(uint16_t* port) const
{
    if (socketMode & Unix)
        return String();
    return getNameHelper(fd, ::getsockname, port);
}

bool SocketClient::writeTo(const String& host, uint16_t port, const unsigned char* f_data, unsigned int size)
{
#ifdef _WIN32
    const char *data = reinterpret_cast<const char*>(f_data);
#else
    const unsigned char *data = f_data;
#endif

    if (size) {
        mWrites.append(size);
    }

    assert((!size) == (!data));
    SocketClient::SharedPtr socketPtr = shared_from_this();

    Resolver resolver;
    if (port != 0)
        resolver.resolve(host, port, socketPtr);

    int e;
    unsigned int total = 0;

#ifdef HAVE_NOSIGNAL
    const int sendFlags = MSG_NOSIGNAL;
#else
    const int sendFlags = 0;
#endif

    if (!writeWait) {
        if (!writeBuffer.isEmpty()) {
            assert(writeOffset < writeBuffer.size());
            const size_t writeBufferSize = writeBuffer.size() - writeOffset;
            while (total < writeBufferSize) {
                assert(writeBuffer.size() > total);
                if (resolver.addr) {
                    eintrwrap(e, ::sendto(fd, reinterpret_cast<const char*>(writeBuffer.data()) + total + writeOffset, writeBufferSize - total,
                                          sendFlags, resolver.addr, resolver.size));
                } else {
                    eintrwrap(e, ::write(fd, writeBuffer.data() + total + writeOffset, writeBufferSize - total));
                }
                DEBUG() << "SENT(1)" << (writeBufferSize - total) << "BYTES" << e << errno;
                if (e == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        if (wMode == Synchronous) {
                            if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
                                if (total) {
                                    if (total < writeBufferSize) {
                                        writeOffset += total;
                                    } else {
                                        writeOffset = 0;
                                        writeBuffer.clear();
                                    }
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
                assert(total <= writeBufferSize);
                if (total < writeBufferSize) {
                    writeOffset += total;
                } else {
                    writeOffset = 0;
                    writeBuffer.clear();
                }
            }
        }

        if (fd == -1 || !data) {
            return fd != -1;
        }
        total = 0;

        assert(data != 0 && size > 0);

        if (writeBuffer.isEmpty()) {
            for (;;) {
                assert(size > total);
                if (resolver.addr) {
                    eintrwrap(e, ::sendto(fd, data + total, size - total,
                                          sendFlags, resolver.addr, resolver.size));
                } else {
                    eintrwrap(e, ::write(fd, data + total, size - total));
                }
                DEBUG() << "SENT(2)" << (size - total) << "BYTES" << e << errno;
                if (e == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        if (wMode == Synchronous) {
                            if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
                                // store the rest
                                const unsigned int rem = size - total;
                                writeBuffer.reserve(writeBuffer.size() + rem);
                                memcpy(writeBuffer.end(), data + total, rem);
                                writeBuffer.resize(writeBuffer.size() + rem);
                                assert(!writeOffset);

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
    }

    if (total < size) {
        // store the rest
        const unsigned int rem = size - total;
        writeBuffer.reserve(writeBuffer.size() + rem);
        memcpy(writeBuffer.end(), data + total, rem);
        writeBuffer.resize(writeBuffer.size() + rem);
    }
    return true;
}

bool SocketClient::write(const void *data, unsigned int size)
{
    return writeTo(String(), 0, reinterpret_cast<const unsigned char*>(data), size);
}

static String addrToString(const sockaddr* addr, bool IPv6)
{
    String ip(INET6_ADDRSTRLEN, '\0');
    if (IPv6) {
        const sockaddr_in6* addr6 = reinterpret_cast<const sockaddr_in6*>(addr);
#ifdef _WIN32
        // stupid windows declares inet_ntop wrong: 3rd argument is PVOID,
        // which is not const -- we have to cast the const away :(
        PVOID input = const_cast<PVOID>(static_cast<const void*>(&addr6->sin6_addr));
#else
        const void *input = &addr6->sin6_addr;
#endif
        inet_ntop(AF_INET6, input, &ip[0], ip.size());
    } else {
        const sockaddr_in* addr4 = reinterpret_cast<const sockaddr_in*>(addr);
#ifdef _WIN32
        // stupid windows declares inet_ntop wrong: 3rd argument is PVOID,
        // which is not const -- we have to cast the const away :(
        PVOID input = const_cast<PVOID>(static_cast<const void*>(&addr4->sin_addr));
#else
        const void *input = &addr4->sin_addr;
#endif
        inet_ntop(AF_INET, input, &ip[0], ip.size());
    }
    ip.resize(strlen(ip.constData()));
    return ip;
}

static uint16_t addrToPort(const sockaddr* addr, bool IPv6)
{
    if (IPv6)
        return ntohs(reinterpret_cast<const sockaddr_in6*>(addr)->sin6_port);
    return ntohs(reinterpret_cast<const sockaddr_in*>(addr)->sin_port);
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

    union {
        sockaddr_in fromAddr4;
        sockaddr_in6 fromAddr6;
        sockaddr fromAddr;
    };

    socklen_t fromLen = 0;
    const bool isIPv6 = socketMode & IPv6;

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
            if (socketMode & Udp) {
                if (isIPv6) {
                    fromLen = sizeof(fromAddr6);
                    eintrwrap(e, ::recvfrom(fd, reinterpret_cast<char*>(readBuffer.end()), rem, 0, &fromAddr, &fromLen));
                } else {
                    fromLen = sizeof(fromAddr4);
                    eintrwrap(e, ::recvfrom(fd, reinterpret_cast<char*>(readBuffer.end()), rem, 0, &fromAddr, &fromLen));
                }
            } else {
                eintrwrap(e, ::read(fd, readBuffer.end(), rem));
            }
            DEBUG() << "RECEIVED(2)" << rem << "BYTES" << e << errno;
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
                    if (!fromLen)
                        signalReadyRead(socketPtr, std::move(readBuffer));
                }
                signalDisconnected(socketPtr);
                close();
                return;
            } else if (fromLen) {
                readBuffer.resize(e);
                signalReadyReadFrom(socketPtr, addrToString(&fromAddr, isIPv6), addrToPort(&fromAddr, isIPv6), std::move(readBuffer));
                readBuffer.clear();
            } else {
                total += e;
                readBuffer.resize(total);
            }
        }
        assert(total <= readBuffer.capacity());
        if (!fromLen)
            signalReadyRead(socketPtr, std::move(readBuffer));

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

            int e = ::getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &size);

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

bool SocketClient::init(unsigned int mode)
{
    int domain = -1, type = -1;
    switch (mode & (Udp|Tcp|Unix)) {
    case Udp:
        type = SOCK_DGRAM;
        domain = (mode & IPv6) ? AF_INET6 : AF_INET;
        break;
    case Tcp:
        type = SOCK_STREAM;
        domain = (mode & IPv6) ? AF_INET6 : AF_INET;
        break;
    case Unix:
        type = SOCK_STREAM;
        assert(!(mode & IPv6));
        domain = PF_UNIX;
        break;
    }

    fd = ::socket(domain, type, 0);
    if (fd < 0) {
        // bad
        return false;
    }
#ifdef HAVE_NOSIGPIPE
    int flags = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&flags, sizeof(int));
#endif
#ifdef HAVE_CLOEXEC
    setFlags(fd, FD_CLOEXEC, F_GETFD, F_SETFD);
#endif

    if (!blocking) {
        if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
            loop->registerSocket(fd, EventLoop::SocketRead,
                                 std::bind(&SocketClient::socketCallback, this, std::placeholders::_1, std::placeholders::_2));
#ifndef _WIN32   // no O_NONBLOCK on windows
            if (!setFlags(fd, O_NONBLOCK, F_GETFL, F_SETFL)) {
                close();
                return false;
            }
#endif
        }
    }

    socketMode = mode;
    return true;
}

bool SocketClient::setFlags(int fd, int flag, int getcmd, int setcmd, FlagMode mode)
{
#ifdef _WIN32
    (void) fd; (void) flag; (void) getcmd; (void) setcmd; (void) mode;  // unused
    return false;  // no fcntl() on windows
#else
    int flg = 0, e;
    if (mode == FlagAppend) {
        eintrwrap(e, ::fcntl(fd, getcmd, 0));
        if (e == -1)
            return false;
        flg = e;
    }
    flg |= flag;
    eintrwrap(e, ::fcntl(fd, setcmd, flg));
    return e != -1;
#endif
}

double SocketClient::mbpsWritten() const
{
    uint64_t bytes = 0, currentStart = 0, currentEnd = 0;
    uint64_t elapsed = 0;

    for (const auto &t : mWrites) {
        // assert(t.isCompleted());
        bytes += t.bytes;
        if (t.startTime > currentEnd) {
            elapsed += currentEnd - currentStart;
            currentStart = t.startTime;
        }
        assert(currentEnd >= currentStart);
        currentEnd = t.endTime;
    }
    elapsed += currentEnd - currentStart;
    return ((static_cast<double>(bytes) / (1024 * 1024)) / (static_cast<double>(elapsed) / 1000.0));
}


void SocketClient::bytesWritten(const SocketClient::SharedPtr &socket, uint64_t bytes)
{
    const uint64_t copy = bytes;
    List<TimeData>::iterator it = mPendingWrites.begin();
    while (bytes && it != mPendingWrites.end()) {
        if (it->add(bytes)) {
            mWrites.append(*it);
            it = mPendingWrites.erase(it);
        } else {
            ++it;
        }
    }
    assert(!bytes);
    signalBytesWritten(socket, copy);
}

