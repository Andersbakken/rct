#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <functional>
#include <utility>

#include "Buffer.h"
#include "Rct.h"
#include "SignalSlot.h"
#include "String.h"

// #define RCT_SOCKETCLIENT_TIMING_ENABLED
class SocketClient : public std::enable_shared_from_this<SocketClient>
{
public:
    enum Mode {
        None = 0x0,
        Tcp = 0x1,
        Udp = 0x2,
        Unix = 0x4,
        IPv6 = 0x8,
        Blocking = 0x10
    };

    SocketClient(unsigned int mode = 0);
    SocketClient(int fd, unsigned int mode);
    ~SocketClient();

    int takeFD() { const int f = mFd; mFd = -1; return f; }

    enum State { Disconnected, Connecting, Connected };
    State state() const { return mSocketState; }
    unsigned int mode() const { return mSocketMode; }
#ifndef _WIN32
    bool connect(const String &path); // UNIX
#endif
    bool connect(const String &host, uint16_t port); // TCP
    bool bind(uint16_t port); // UDP

    String hostName() const { return (mSocketMode & Tcp ? mAddress : String()); }
    String path() const { return (mSocketMode & Unix ? mAddress : String()); }
    uint16_t port() const { return mSocketPort; }

    bool isConnected() const { return mFd != -1; }
    int socket() const { return mFd; }

    void close();

    // TCP/UNIX
    bool write(const void *data, unsigned int num);
    bool write(const String &data) { return write(&data[0], data.size()); }

    String peerName(uint16_t *port = nullptr) const;
    String peerString() const
    {
        uint16_t port;
        const String name = peerName(&port);
        if (!name.isEmpty()) {
            return String::format<64>("%s:%u", name.constData(), port);
        }
        return String();
    }
    String sockName(uint16_t *port = nullptr) const;
    String sockString() const
    {
        uint16_t port;
        const String name = sockName(&port);
        if (!name.isEmpty()) {
            return String::format<64>("%s:%u", name.constData(), port);
        }
        return String();
    }

    // UDP
    bool writeTo(const String &host, uint16_t port, const unsigned char *data, unsigned int num);
    bool writeTo(const String &host, uint16_t port, const String &data)
    {
        return writeTo(host, port, reinterpret_cast<const unsigned char *>(&data[0]), data.size());
    }

    // UDP Multicast
    bool addMembership(const String &ip);
    bool dropMembership(const String &ip);
    void setMulticastLoop(bool loop);
    void setMulticastTTL(unsigned char ttl);

    const Buffer &buffer() const { return mReadBuffer; }
    Buffer &&takeBuffer() { return std::move(mReadBuffer); }

    Signal<std::function<void(const std::shared_ptr<SocketClient>&, Buffer&&)> >& readyRead() { return mSignalReadyRead; }
    Signal<std::function<void(const std::shared_ptr<SocketClient>&, const String&, uint16_t, Buffer&&)> >& readyReadFrom() { return mSignalReadyReadFrom; }
    Signal<std::function<void(const std::shared_ptr<SocketClient>&)> >& connected() { return signalConnected; }
    Signal<std::function<void(const std::shared_ptr<SocketClient>&)> >& disconnected() { return signalDisconnected; }
    Signal<std::function<void(const std::shared_ptr<SocketClient>&, int)> >& bytesWritten() { return mSignalBytesWritten; }

    enum Error {
        InitializeError,
        DnsError,
        ConnectError,
        BindError,
        ReadError,
        WriteError,
        EventLoopError
    };
    Signal<std::function<void(const std::shared_ptr<SocketClient>&, Error)> >& error() { return mSignalError; }

    enum FlagMode { FlagAppend, FlagOverwrite };
    static bool setFlags(int fd, int flag, int getcmd, int setcmd, FlagMode mode = FlagAppend);

    size_t maxWriteBufferSize() const { return mMaxWriteBufferSize; }
    void setMaxWriteBufferSize(size_t maxWriteBufferSize) { mMaxWriteBufferSize = maxWriteBufferSize; }

#ifdef RCT_SOCKETCLIENT_TIMING_ENABLED
    double mbpsWritten() const;
#endif
    bool logsEnabled() const { return mLogsEnabled; }
    void setLogsEnabled(bool on) { mLogsEnabled = on; }
private:
    bool init(unsigned int mode);

    int mFd { -1 };
    uint16_t mSocketPort { 0 };
    State mSocketState { Disconnected };
    unsigned int mSocketMode { None };
    bool mWriteWait { false };
    String mAddress;
    bool mBlocking { false };
    bool mLogsEnabled { true };
    size_t mMaxWriteBufferSize { 0 };

    Signal<std::function<void(const std::shared_ptr<SocketClient>&, Buffer&&)> > mSignalReadyRead;
    Signal<std::function<void(const std::shared_ptr<SocketClient>&, const String&, uint16_t, Buffer&&)> > mSignalReadyReadFrom;
    Signal<std::function<void(const std::shared_ptr<SocketClient>&)> >signalConnected, signalDisconnected;
    Signal<std::function<void(const std::shared_ptr<SocketClient>&, Error)> > mSignalError;
    Signal<std::function<void(const std::shared_ptr<SocketClient>&, int)> > mSignalBytesWritten;
    void bytesWritten(const std::shared_ptr<SocketClient> &socket, uint64_t bytes);
    Buffer mReadBuffer, mWriteBuffer;
    size_t mWriteOffset;

    int writeData(const unsigned char *data, int size);
    void socketCallback(int, int);

#ifdef RCT_SOCKETCLIENT_TIMING_ENABLED
    struct TimeData {
        TimeData(uint64_t b = 0)
            : startTime(Rct::monoMs()), endTime(0), bytes(b), completed(0)
        {}

        bool add(uint64_t &available)
        {
            assert(completed < bytes);
            const uint64_t needed = bytes - completed;
            const uint64_t provided = std::min(needed, available);
            completed += provided;
            assert(available > provided);
            available -= provided;
            if (needed == provided) {
                endTime = Rct::monoMs();
                return true;
            }
            return false;
        }

        bool isCompleted() const
        {
            return bytes == completed;
        }
        uint64_t startTime, endTime, bytes, completed;
    };

    void compactWrites(size_t maxSize)
    {
        if (mWrites.size() > maxSize) {
            mWrites.remove(0, mWrites.size() - maxSize);
        }
    }

    List<TimeData> mWrites, mPendingWrites;
#endif

    friend class Resolver;
};

#endif
