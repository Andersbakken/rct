#include "rct/SocketClient.h"
#include "rct/Event.h"
#include "rct/EventLoop.h"
#include "rct/Log.h"
#include "rct/StopWatch.h"
#include "rct/Rct.h"
#include <algorithm>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>

class DelayedWriteEvent : public Event
{
public:
    enum { Type = 1 };
    DelayedWriteEvent(const String& d)
        : Event(Type), data(d)
    {}

    const String data;
};

static pthread_once_t sigPipeHandler = PTHREAD_ONCE_INIT;

static void initSigPipe()
{
    signal(SIGPIPE, SIG_IGN);
}

SocketClient::SocketClient()
    : mFd(-1), mBufferIdx(0), mReadBufferPos(0)
{
    pthread_once(&sigPipeHandler, initSigPipe);
}

SocketClient::SocketClient(int fd)
    : mFd(fd), mBufferIdx(0), mReadBufferPos(0)
{
    pthread_once(&sigPipeHandler, initSigPipe);
    int flags;
    eintrwrap(flags, fcntl(mFd, F_GETFL, 0));
    eintrwrap(flags, fcntl(mFd, F_SETFL, flags | O_NONBLOCK));
#ifdef HAVE_NOSIGPIPE
    flags = 1;
    ::setsockopt(mFd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&flags, sizeof(int));
#endif
    EventLoop::instance()->addFileDescriptor(mFd, EventLoop::Read, dataCallback, this);
}

SocketClient::~SocketClient()
{
    disconnect();
}

static inline bool connectInternal(int& fd, int domain, sockaddr* address, size_t addressSize, int maxTime)
{
    StopWatch timer;

    while (true) {
        fd = ::socket(domain, SOCK_STREAM, 0);
        if (fd == -1) {
            return false;
        }
        int ret;
        eintrwrap(ret, ::connect(fd, address, addressSize));
        if (!ret) {
#ifdef HAVE_NOSIGPIPE
            ret = 1;
            ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&ret, sizeof(int));
#endif
            break;
        }
        eintrwrap(ret, ::close(fd));
        fd = -1;
        if (maxTime > 0 && timer.elapsed() >= maxTime) {
            return false;
        }
        usleep(100000);
    }

    assert(fd != -1);
    int flags;
    eintrwrap(flags, fcntl(fd, F_GETFL, 0));
    eintrwrap(flags, fcntl(fd, F_SETFL, flags | O_NONBLOCK));

    return true;
}

bool SocketClient::connectTcp(const String& host, uint16_t port, int maxTime)
{
    addrinfo *result;
    if (getaddrinfo(host.nullTerminated(), 0, 0, &result))
        return false;

    sockaddr_in* addr = 0;
    addrinfo* cur = result;
    while (cur) {
        if (cur->ai_family == AF_INET) {
            addr = reinterpret_cast<sockaddr_in*>(cur->ai_addr);
            break;
        }
        cur = cur->ai_next;
    }

    bool ok = false;
    if (addr) {
        addr->sin_port = htons(port);
        if (connectInternal(mFd, AF_INET, reinterpret_cast<sockaddr*>(addr), sizeof(sockaddr_in), maxTime)) {
            ok = true;

            unsigned int fdflags = EventLoop::Read;
            if (!mBuffers.empty())
                fdflags |= EventLoop::Write;
            EventLoop::instance()->addFileDescriptor(mFd, fdflags, dataCallback, this);

            mConnected(this);
        }
    }

    freeaddrinfo(result);

    return ok;
}

bool SocketClient::connectUnix(const Path& path, int maxTime)
{
    sockaddr_un unAddress;
    memset(&unAddress, 0, sizeof(sockaddr_un));

    unAddress.sun_family = AF_UNIX;
    const int sz = std::min<int>(sizeof(unAddress.sun_path) - 1, path.size());
    memcpy(unAddress.sun_path, path.constData(), sz);
    unAddress.sun_path[sz] = '\0';

    if (connectInternal(mFd, PF_UNIX, reinterpret_cast<sockaddr*>(&unAddress), sizeof(sockaddr_un), maxTime)) {
        unsigned int fdflags = EventLoop::Read;
        if (!mBuffers.empty())
            fdflags |= EventLoop::Write;
        EventLoop::instance()->addFileDescriptor(mFd, fdflags, dataCallback, this);

        mConnected(this);
        return true;
    }

    return false;
}

void SocketClient::disconnect()
{
    if (mFd != -1) {
        int ret;
        eintrwrap(ret, ::close(mFd));
        EventLoop::instance()->removeFileDescriptor(mFd);
        mFd = -1;
        mDisconnected(this);
    }
}

void SocketClient::dataCallback(int, unsigned int flags, void* userData)
{
    SocketClient* client = reinterpret_cast<SocketClient*>(userData);
    if (flags & EventLoop::Read)
        client->readMore();
    if (flags & EventLoop::Write)
        client->writeMore();
}

String SocketClient::readAll()
{
    String buf;
    std::swap(buf, mReadBuffer);
    if (mReadBufferPos) {
        buf.remove(0, mReadBufferPos);
        mReadBufferPos = 0;
    }
    return buf;
}

int SocketClient::read(char *buf, int size)
{
    size = std::min(bytesAvailable(), size);
    if (size) {
        memcpy(buf, mReadBuffer.data() + mReadBufferPos, size);
        mReadBufferPos += size;
        if (mReadBuffer.size() == mReadBufferPos) {
            mReadBuffer.clear();
            mReadBufferPos = 0;
        }
    }
    return size;
}

bool SocketClient::write(const String& data)
{
    if (pthread_equal(pthread_self(), EventLoop::instance()->thread())) {
        if (mBuffers.empty())
            EventLoop::instance()->addFileDescriptor(mFd, EventLoop::Read | EventLoop::Write, dataCallback, this);
        mBuffers.push_back(data);
        return writeMore();
    } else {
        EventLoop::instance()->postEvent(this, new DelayedWriteEvent(data));
        return true;
    }
}

void SocketClient::readMore()
{
    enum { BufSize = 1024, MaxBufferSize = 1024 * 1024 * 16 };

#ifdef HAVE_NOSIGNAL
    const int recvflags = MSG_NOSIGNAL;
#else
    const int recvflags = 0;
#endif
    char buf[BufSize];
    int read = 0;
    bool wasDisconnected = false;
    for (;;) {
        int r;
        eintrwrap(r, ::recv(mFd, buf, BufSize, recvflags));

        if (r == -1) {
            break;
        } else if (!r) {
            wasDisconnected = true;
            break;
        }
        read += r;
        mReadBuffer.resize(r + mReadBuffer.size());
        memcpy(mReadBuffer.data() + mReadBuffer.size() - r, buf, r);
        if (mReadBuffer.size() + r >= MaxBufferSize) {
            if (mReadBuffer.size() + r - mReadBufferPos < MaxBufferSize) {
                mReadBuffer.remove(0, mReadBufferPos);
                mReadBufferPos = 0;
            } else {
                error("Buffer exhausted (%d), dropping on the floor", mReadBuffer.size());
                mReadBuffer.clear();
            }
        }
    }

    if (read && !mReadBuffer.isEmpty())
        mDataAvailable(this);
    if (wasDisconnected)
        disconnect();
}

bool SocketClient::writeMore()
{
    bool ret = true;
    int written = 0;
#ifdef HAVE_NOSIGNAL
    const int sendflags = MSG_NOSIGNAL;
#else
    const int sendflags = 0;
#endif
    for (;;) {
        if (mBuffers.empty()) {
            EventLoop::instance()->removeFileDescriptor(mFd, EventLoop::Write);
            break;
        }
        const String& front = mBuffers.front();
        int w;
        eintrwrap(w, ::send(mFd, &front[mBufferIdx], front.size() - mBufferIdx, sendflags));

        if (w == -1) {
            ret = (errno == EWOULDBLOCK || errno == EAGAIN); // apparently these can be different
            break;
        }
        written += w;
        mBufferIdx += w;
        if (mBufferIdx == front.size()) {
            assert(!mBuffers.empty());
            mBuffers.pop_front();
            mBufferIdx = 0;
            continue;
        }
    }
    if (written)
        mBytesWritten(this, written);
    return ret;
}

void SocketClient::event(const Event* event)
{
    switch (event->type()) {
    case DelayedWriteEvent::Type: {
        const DelayedWriteEvent *ev = static_cast<const DelayedWriteEvent*>(event);
        assert(pthread_equal(pthread_self(), EventLoop::instance()->thread()));
        if (mBuffers.empty())
            EventLoop::instance()->addFileDescriptor(mFd, EventLoop::Read | EventLoop::Write, dataCallback, this);
        mBuffers.push_back(ev->data);
        writeMore();
        break; }
    default:
        EventReceiver::event(event);
    }
}
