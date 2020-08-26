#include "SocketClient.h"

#ifdef _WIN32
#  include <Winsock2.h>
#  include <Ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <sys/un.h>
#endif
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <cstdint>
#include <map>

#include "EventLoop.h"
#include "rct/rct-config.h"
#include "Rct.h"
#include "rct/Buffer.h"
#include "rct/SignalSlot.h"
#include "rct/String.h"

#ifdef NDEBUG
struct Null { template <typename T> Null operator<<(const T &) { return *this; } };
#define DEBUG() if (false) Null()
#else
#define DEBUG() if (mLogsEnabled) debug()
#endif

SocketClient::SocketClient(unsigned int mode)
    : mSocketMode(mode), mBlocking(mode & Blocking), mWriteOffset(0)
{
}

SocketClient::SocketClient(int f, unsigned int mode)
    : mFd(f), mSocketState(Connected), mSocketMode(mode), mWriteOffset(0)
{
    assert(mFd >= 0);
#ifdef HAVE_NOSIGPIPE
    int flags = 1;
    ::setsockopt(mFd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&flags, sizeof(int));
#endif
#ifdef HAVE_CLOEXEC
    setFlags(mFd, FD_CLOEXEC, F_GETFD, F_SETFD);
#endif
    mBlocking = (mode & Blocking);

    if (!mBlocking) {
        if (std::shared_ptr<EventLoop> loop = EventLoop::eventLoop()) {
            loop->registerSocket(mFd, EventLoop::SocketRead,
                                 std::bind(&SocketClient::socketCallback, this, std::placeholders::_1, std::placeholders::_2));
#ifndef _WIN32
            if (!setFlags(mFd, O_NONBLOCK, F_GETFL, F_SETFL)) {
                mSignalError(shared_from_this(), InitializeError);
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
    if (mFd == -1)
        return;
    mSocketState = Disconnected;
    if (!mBlocking) {
        if (std::shared_ptr<EventLoop> loop = EventLoop::eventLoop())
            loop->unregisterSocket(mFd);
    }
    ::close(mFd);
    mSocketPort = 0;
    mAddress.clear();
    mFd = -1;
}

class Resolver
{
public:
    Resolver();
    Resolver(const String& host, uint16_t port, const std::shared_ptr<SocketClient>& socket);
    ~Resolver();

    void resolve(const String& host, uint16_t port, const std::shared_ptr<SocketClient>& socket);

    addrinfo* res;
    sockaddr* addr;
    size_t size;
};

Resolver::Resolver()
    : res(nullptr), addr(nullptr), size(0)
{
}

Resolver::Resolver(const String& host, uint16_t port, const std::shared_ptr<SocketClient>& socket)
    : res(nullptr), addr(nullptr), size(0)
{
    resolve(host, port, socket);
}

void Resolver::resolve(const String& host, uint16_t port, const std::shared_ptr<SocketClient>& socket)
{
    // first, see if this parses as an IP mAddress
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

    // not an ip mAddress, try to resolve it
    addrinfo hints, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.constData(), nullptr, &hints, &res) != 0) {
        // bad
        socket->mSignalError(socket, SocketClient::DnsError);
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
        socket->mSignalError(socket, SocketClient::DnsError);
        socket->close();
        freeaddrinfo(res);
        res = nullptr;
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
    std::shared_ptr<SocketClient> tcpSocket = shared_from_this();
    Resolver resolver(host, port, tcpSocket);
    if (!resolver.addr)
        return false;

    unsigned int mode = Tcp;
    if (resolver.size == sizeof(sockaddr_in6))
        mode |= IPv6;

    if (!init(mode))
        return false;

    int e;
    eintrwrap(e, ::connect(mFd, resolver.addr, resolver.size));
    mSocketPort = port;
    mAddress = host;
    if (e == 0) { // we're done
        mSocketState = Connected;

        signalConnected(tcpSocket);
    } else {
        if (errno != EINPROGRESS) {
            // bad
            mSignalError(tcpSocket, ConnectError);
            close();
            return false;
        }
        if (std::shared_ptr<EventLoop> loop = EventLoop::eventLoop()) {
            loop->updateSocket(mFd, EventLoop::SocketRead|EventLoop::SocketWrite|EventLoop::SocketOneShot);
            mWriteWait = true;
        }
        mSocketState = Connecting;
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
    if (path.size() >= sizeof(addr_un.sun_path))
        return false;
    memset(&addr_un, '\0', sizeof(addr_un));
    addr_un.sun_family = AF_UNIX;
    strncpy(addr_un.sun_path, path.constData(), sizeof(addr_un.sun_path) - 1);

    std::shared_ptr<SocketClient> unixSocket = shared_from_this();

    int e;
    eintrwrap(e, ::connect(mFd, &addr, sizeof(addr_un)));
    mAddress = path;
    if (e == 0) { // we're done
        mSocketState = Connected;

        signalConnected(unixSocket);
    } else {
        if (errno != EINPROGRESS) {
            // bad
            mSignalError(unixSocket, ConnectError);
            close();
            return false;
        }
        if (std::shared_ptr<EventLoop> loop = EventLoop::eventLoop()) {
            loop->updateSocket(mFd, EventLoop::SocketRead|EventLoop::SocketWrite|EventLoop::SocketOneShot);
            mWriteWait = true;
        }
        mSocketState = Connecting;
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
    if (mSocketMode & IPv6) {
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
    e = ::setsockopt(mFd, SOL_SOCKET, SO_REUSEADDR, pin, sizeof(in));
    if (e == -1) {
        std::shared_ptr<SocketClient> udpSocket = shared_from_this();
        mSignalError(udpSocket, BindError);
        close();
        return false;
    }
    e = ::bind(mFd, &addr, size);
    if (!e) {
        mSocketPort = port;
        return true;
    }

    std::shared_ptr<SocketClient> udpSocket = shared_from_this();
    mSignalError(udpSocket, BindError);
    close();
    return false;
}

bool SocketClient::addMembership(const String& ip)
{
    struct ip_mreq mreq;
    if (inet_pton(AF_INET, ip.constData(), &mreq.imr_multiaddr) == 0)
        return false;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    ::setsockopt(mFd, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq));
    return true;
}

bool SocketClient::dropMembership(const String& ip)
{
    struct ip_mreq mreq;
    if (inet_pton(AF_INET, ip.constData(), &mreq.imr_multiaddr) == 0)
        return false;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    ::setsockopt(mFd, IPPROTO_IP, IP_DROP_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq));
    return true;
}

void SocketClient::setMulticastLoop(bool loop)
{
    const char ena = loop ? 1 : 0;
    ::setsockopt(mFd, IPPROTO_IP, IP_MULTICAST_LOOP, &ena, sizeof(ena));
}

void SocketClient::setMulticastTTL(unsigned char ttl)
{
    ::setsockopt(mFd, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<char*>(&ttl), sizeof(ttl));
}

#ifdef _WIN32
typedef int (*GetNameFunc)(SOCKET, sockaddr*, socklen_t*);
#else
typedef int (*GetNameFunc)(int, sockaddr*, socklen_t*);
#endif
static inline String getNameHelper(int mFd, GetNameFunc func, uint16_t* port)
{
    union {
        sockaddr_storage storage;
        sockaddr_in6 addr6;
        sockaddr_in addr4;
        sockaddr addr;
    };
    socklen_t size = sizeof(storage);
    if (func(mFd, &addr, &size) == -1)
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
    if (mSocketMode & Unix)
        return String();
    return getNameHelper(mFd, ::getpeername, port);
}

String SocketClient::sockName(uint16_t* port) const
{
    if (mSocketMode & Unix)
        return String();
    return getNameHelper(mFd, ::getsockname, port);
}

bool SocketClient::writeTo(const String& host, uint16_t port, const unsigned char* f_data, unsigned int size)
{
#ifdef _WIN32
    const char *data = reinterpret_cast<const char*>(f_data);
#else
    const unsigned char *data = f_data;
#endif

#ifdef RCT_SOCKETCLIENT_TIMING_ENABLED
    if (size) {
        mWrites.append(size);
    }
#endif

    assert((!size) == (!data));
    std::shared_ptr<SocketClient> socketPtr = shared_from_this();

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

    if (!mWriteWait) {
        if (!mWriteBuffer.isEmpty()) {
            // assert(mWriteOffset < mWriteBuffer.size());
            const size_t writeBufferSize = mWriteBuffer.size() - mWriteOffset;
            while (total < writeBufferSize) {
                assert(mWriteBuffer.size() > total);
                if (resolver.addr) {
                    eintrwrap(e, ::sendto(mFd, reinterpret_cast<const char*>(mWriteBuffer.data()) + total + mWriteOffset, writeBufferSize - total,
                                          sendFlags, resolver.addr, resolver.size));
                } else {
                    eintrwrap(e, ::write(mFd, mWriteBuffer.data() + total + mWriteOffset, writeBufferSize - total));
                }
                DEBUG() << "SENT(1)" << (writeBufferSize - total) << "BYTES" << e << errno;
                if (e == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        assert(!mWriteWait);
                        if (std::shared_ptr<EventLoop> loop = EventLoop::eventLoop()) {
                            loop->updateSocket(mFd, EventLoop::SocketRead|EventLoop::SocketWrite|EventLoop::SocketOneShot);
                            mWriteWait = true;
                        }
                        break;
                    } else {
                        // bad
                        mSignalError(shared_from_this(), WriteError);
                        close();
                        return false;
                    }
                }
                mSignalBytesWritten(socketPtr, e);
                total += e;
            }
            if (total) {
                assert(total <= writeBufferSize);
                if (total < writeBufferSize) {
                    mWriteOffset += total;
                } else {
                    mWriteOffset = 0;
                    mWriteBuffer.clear();
                }
            }
        }

        if (mFd == -1 || !data) {
            return mFd != -1;
        }
        total = 0;

        assert(data != nullptr && size > 0);

        if (mWriteBuffer.isEmpty()) {
            for (;;) {
                assert(size > total);
                if (resolver.addr) {
                    eintrwrap(e, ::sendto(mFd, data + total, size - total,
                                          sendFlags, resolver.addr, resolver.size));
                } else {
                    eintrwrap(e, ::write(mFd, data + total, size - total));
                }
                DEBUG() << "SENT(2)" << (size - total) << "BYTES" << e << errno;
                if (e == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        assert(!mWriteWait);
                        if (std::shared_ptr<EventLoop> loop = EventLoop::eventLoop()) {
                            loop->updateSocket(mFd, EventLoop::SocketRead|EventLoop::SocketWrite|EventLoop::SocketOneShot);
                            mWriteWait = true;
                        }
                        break;
                    } else {
                        // bad
                        mSignalError(shared_from_this(), WriteError);
                        close();
                        return false;
                    }
                }
                mSignalBytesWritten(socketPtr, e);
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
        if (mMaxWriteBufferSize && mWriteBuffer.size() + rem > mMaxWriteBufferSize) {
            close();
            return false;
        }
        mWriteBuffer.reserve(mWriteBuffer.size() + rem);
        memcpy(mWriteBuffer.end(), data + total, rem);
        mWriteBuffer.resize(mWriteBuffer.size() + rem);
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
    assert(f == mFd);
    (void)f;

    std::shared_ptr<SocketClient> socketPtr = shared_from_this();

    if (mode == EventLoop::SocketError) {
        mSignalError(socketPtr, EventLoopError);
        close();
        return;
    }

    if (mWriteWait && (mode & EventLoop::SocketWrite)) {
        if (std::shared_ptr<EventLoop> loop = EventLoop::eventLoop()) {
            loop->updateSocket(mFd, EventLoop::SocketRead);
            mWriteWait = false;
        }
    }

    union {
        sockaddr_in fromAddr4;
        sockaddr_in6 fromAddr6;
        sockaddr fromAddr;
    };

    socklen_t fromLen = 0;
    const bool isIPv6 = mSocketMode & IPv6;

    if (mode & EventLoop::SocketRead) {

        enum { BlockSize = 1024, AllocateAt = 512 };
        int e;

        unsigned int total = 0;
        for(;;) {
            unsigned int rem = mReadBuffer.capacity() - mReadBuffer.size();
            // printf("reading, remaining size %u\n", rem);
            if (rem <= AllocateAt) {
                // printf("allocating more\n");
                mReadBuffer.reserve(mReadBuffer.size() + BlockSize);
                rem = mReadBuffer.capacity() - mReadBuffer.size();
                // printf("Rem is now %d\n", rem);
            }
            if (mSocketMode & Udp) {
                if (isIPv6) {
                    fromLen = sizeof(fromAddr6);
                    eintrwrap(e, ::recvfrom(mFd, reinterpret_cast<char*>(mReadBuffer.end()), rem, 0, &fromAddr, &fromLen));
                } else {
                    fromLen = sizeof(fromAddr4);
                    eintrwrap(e, ::recvfrom(mFd, reinterpret_cast<char*>(mReadBuffer.end()), rem, 0, &fromAddr, &fromLen));
                }
            } else {
                eintrwrap(e, ::read(mFd, mReadBuffer.end(), rem));
            }
            DEBUG() << "RECEIVED(2)" << rem << "BYTES" << e << errno;
            if (e == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    // bad
                    mSignalError(socketPtr, ReadError);
                    close();
                    return;
                }
            } else if (e == 0) {
                // socket closed
                if (total) {
                    if (!fromLen)
                        mSignalReadyRead(socketPtr, std::move(mReadBuffer));
                }
                signalDisconnected(socketPtr);
                close();
                return;
            } else if (fromLen) {
                mReadBuffer.resize(e);
                mSignalReadyReadFrom(socketPtr, addrToString(&fromAddr, isIPv6), addrToPort(&fromAddr, isIPv6), std::move(mReadBuffer));
                mReadBuffer.clear();
            } else {
                total += e;
                mReadBuffer.resize(total);
            }
        }
        assert(total <= mReadBuffer.capacity());
        if (!fromLen)
            mSignalReadyRead(socketPtr, std::move(mReadBuffer));

        if (mWriteWait) {
            if (std::shared_ptr<EventLoop> loop = EventLoop::eventLoop()) {
                loop->updateSocket(mFd, EventLoop::SocketRead|EventLoop::SocketWrite|EventLoop::SocketOneShot);
            }
        }
    }
    if (mode & EventLoop::SocketWrite) {
        if (mSocketState == Connecting) {
            int err;
            socklen_t size = sizeof(err);

            int e = ::getsockopt(mFd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &size);

            if (e == -1) {
                // bad
                mSignalError(socketPtr, ConnectError);
                close();
                return;
            }
            if (!err) {
                // connected
                mSocketState = Connected;
                signalConnected(socketPtr);
            } else {
                // failed to connect
                mSignalError(socketPtr, ConnectError);
                close();
                return;
            }
        }
        write(nullptr, 0);
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

    mFd = ::socket(domain, type, 0);
    if (mFd < 0) {
        // bad
        return false;
    }
#ifdef HAVE_NOSIGPIPE
    int flags = 1;
    ::setsockopt(mFd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&flags, sizeof(int));
#endif
#ifdef HAVE_CLOEXEC
    setFlags(mFd, FD_CLOEXEC, F_GETFD, F_SETFD);
#endif

    if (!mBlocking) {
        if (std::shared_ptr<EventLoop> loop = EventLoop::eventLoop()) {
            loop->registerSocket(mFd, EventLoop::SocketRead,
                                 std::bind(&SocketClient::socketCallback, this, std::placeholders::_1, std::placeholders::_2));
#ifndef _WIN32   // no O_NONBLOCK on windows
            if (!setFlags(mFd, O_NONBLOCK, F_GETFL, F_SETFL)) {
                close();
                return false;
            }
#endif
        }
    }

    mSocketMode = mode;
    return true;
}

bool SocketClient::setFlags(int mFd, int flag, int getcmd, int setcmd, FlagMode mode)
{
#ifdef _WIN32
    (void) mFd; (void) flag; (void) getcmd; (void) setcmd; (void) mode;  // unused
    return false;  // no fcntl() on windows
#else
    int flg = 0, e;
    if (mode == FlagAppend) {
        eintrwrap(e, ::fcntl(mFd, getcmd, 0));
        if (e == -1)
            return false;
        flg = e;
    }
    flg |= flag;
    eintrwrap(e, ::fcntl(mFd, setcmd, flg));
    return e != -1;
#endif
}

#ifdef RCT_SOCKETCLIENT_TIMING_ENABLED
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
#endif


void SocketClient::bytesWritten(const std::shared_ptr<SocketClient> &socket, uint64_t bytes)
{
    const uint64_t copy = bytes;
#ifdef RCT_SOCKETCLIENT_TIMING_ENABLED
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
#endif
    mSignalBytesWritten(socket, copy);
}
