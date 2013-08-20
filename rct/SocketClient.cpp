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
#include <netinet/in.h>
#include <netdb.h>
#include <rct-config.h>

#define eintrwrap(VAR, BLOCK)                   \
    do {                                        \
        VAR = BLOCK;                            \
    } while (VAR == -1 && errno == EINTR)

SocketClient::SocketClient(Mode mode)
    : socketState(Disconnected), writeWait(false)
{
    fd = ::socket(mode == Tcp ? AF_INET : PF_UNIX, SOCK_STREAM, 0);
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
        printf("%s:%d registerSocket %d\n", __FILE__, __LINE__, fd);
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

SocketClient::SocketClient(int f)
    : fd(f), socketState(Connected), writeWait(false)
{
    assert(fd >= 0);
#ifdef HAVE_NOSIGPIPE
    int flags = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&flags, sizeof(int));
#endif

    if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
        printf("%s:%d registerSocket %d\n", __FILE__, __LINE__, fd);
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

bool SocketClient::connect(const std::string& host, uint16_t port)
{
    addrinfo hints, *res, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;

    SocketClient::SharedPtr tcpSocket = shared_from_this();

    if (getaddrinfo(host.c_str(), NULL, &hints, &res) != 0) {
        // bad
        signalError(tcpSocket, DnsError);
        close();
        return false;
    }

    sockaddr* addr = 0;
    size_t size = 0;
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
        signalError(tcpSocket, DnsError);
        close();
        freeaddrinfo(res);
        return false;
    }

    int e;
    eintrwrap(e, ::connect(fd, addr, size));
    if (e == 0) { // we're done
        socketState = Connected;
        signalConnected(tcpSocket);
    } else {
        if (errno != EINPROGRESS) {
            // bad
            signalError(tcpSocket, ConnectError);
            close();
            freeaddrinfo(res);
            return false;
        }
        if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
            loop->updateSocket(fd, EventLoop::SocketRead|EventLoop::SocketWrite|EventLoop::SocketOneShot);
            writeWait = true;
        }
        socketState = Connecting;
    }

    freeaddrinfo(res); // free the linked list
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

struct Closer
{
    Closer(FILE *f)
        : mF(f)
    {
    }
    ~Closer()
    {
        if (mF)
            fclose(mF);
    }
private:
    FILE *mF;
};

bool SocketClient::write(const unsigned char* data, unsigned int size)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), "/tmp/rtags-%d.write.log", getpid());
    FILE *f = getenv("RTAGS_DUMP") ? fopen(buf, "a") : 0;
    Closer closer(f);
    int e;
    bool done = !data;
    unsigned int total = 0;

    SocketClient::SharedPtr socketPtr = shared_from_this();

    if (!writeWait) {
        if (!writeBuffer.isEmpty()) {
            while (total < writeBuffer.size()) {
                assert(writeBuffer.size() > total);
                eintrwrap(e, ::write(fd, writeBuffer.data() + total, writeBuffer.size() - total));
                if (e == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
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
                if (f)
                    fwrite(writeBuffer.data() + total, e, 1, f);
                fflush(f);
                signalBytesWritten(socketPtr, e);
                total += e;
            }
            if (total && total < writeBuffer.size()) {
                memmove(writeBuffer.data(), writeBuffer.data() + total, writeBuffer.size() - total);
            }
            writeBuffer.resize(writeBuffer.size() - total);
        }

        if (done) {
            return true;
        }
        total = 0;

        assert(data != 0 && size > 0);

        for (;;) {
            assert(size > total);
            eintrwrap(e, ::write(fd, data + total, size - total));
            if (e == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
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
            if (f) {
                fwrite(data + total, e, 1, f);
                fflush(f);
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

void SocketClient::socketCallback(int f, int mode)
{
    assert(f == fd);
    (void)f;

    SocketClient::SharedPtr tcpSocket = shared_from_this();

    if (mode == EventLoop::SocketError) {
        signalError(tcpSocket, EventLoopError);
        close();
        return;
    }

    if (writeWait && (mode & EventLoop::SocketWrite)) {
        if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
            loop->updateSocket(fd, EventLoop::SocketRead);
            writeWait = false;
        }
    }

    char buf[1024];
    snprintf(buf, sizeof(buf), "/tmp/rtags-%d.read.log", getpid());
    FILE *ff = getenv("RTAGS_DUMP") ? fopen(buf, "a") : 0;
    Closer closer(ff);

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
            eintrwrap(e, ::read(fd, readBuffer.end(), rem));
            if (e == -1) {
                if (ff) {
                    fprintf(ff, "\nGot error %d:%s\n", errno, strerror(errno));
                    fflush(ff);
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                else {
                    // bad
                    signalError(tcpSocket, ReadError);
                    close();
                    return;
                }
            } else if (e == 0) {
                if (ff) {
                    fprintf(ff, "\nGot closed\n");
                    fflush(ff);
                }
                // socket closed
                if (total) {
                    signalReadyRead(tcpSocket);
                }
                signalDisconnected(tcpSocket);
                close();
                return;
            } else {
                if (ff) {
                    fwrite(readBuffer.end(), e, 1, ff);
                    fflush(ff);
                }
                total += e;
                readBuffer.resize(total);
                // printf("read %d bytes %d\n", e, total);
            }
        }
        assert(total <= readBuffer.capacity());
        signalReadyRead(tcpSocket);

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
                signalError(tcpSocket, ConnectError);
                close();
                return;
            }
            if (!err) {
                // connected
                socketState = Connected;
                signalConnected(tcpSocket);
            } else {
                // failed to connect
                signalError(tcpSocket, ConnectError);
                close();
                return;
            }
        }
        write(0, 0);
    }
}
