#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include <memory>

#include "Buffer.h"
#include "Rct.h"
#include "SignalSlot.h"
#include "String.h"

class SocketClient : public std::enable_shared_from_this<SocketClient>
{
public:
    typedef std::shared_ptr<SocketClient> SharedPtr;
    typedef std::weak_ptr<SocketClient> WeakPtr;

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

    int takeFD() { const int f = fd; fd = -1; return f; }

    enum State { Disconnected, Connecting, Connected };
    State state() const { return socketState; }
    unsigned int mode() const { return socketMode; }
#ifndef _WIN32
    bool connect(const String& path); // UNIX
#endif
    bool connect(const String& host, uint16_t port); // TCP
    bool bind(uint16_t port); // UDP

    String hostName() const { return (socketMode & Tcp ? address : String()); }
    String path() const { return (socketMode & Unix ? address : String()); }
    uint16_t port() const { return socketPort; }

    bool isConnected() const { return fd != -1; }
    int socket() const { return fd; }

    enum WriteMode {
        Synchronous,
        Asynchronous
    };
    void setWriteMode(WriteMode m) { wMode = m; }
    WriteMode writeMode() const { return wMode; }

    void close();

    // TCP/UNIX
    bool write(const void *data, unsigned int num);
    bool write(const String &data) { return write(&data[0], data.size()); }

    String peerName(uint16_t* port = 0) const;
    String peerString() const
    {
        uint16_t port;
        const String name = peerName(&port);
        if (!name.isEmpty()) {
            return String::format<64>("%s:%u", name.constData(), port);
        }
        return String();
    }
    String sockName(uint16_t* port = 0) const;
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
    bool writeTo(const String& host, uint16_t port, const unsigned char* data, unsigned int num);
    bool writeTo(const String& host, uint16_t port, const String& data)
    {
        return writeTo(host, port, reinterpret_cast<const unsigned char*>(&data[0]), data.size());
    }

    // UDP Multicast
    bool addMembership(const String& ip);
    bool dropMembership(const String& ip);
    void setMulticastLoop(bool loop);
    void setMulticastTTL(unsigned char ttl);

    const Buffer& buffer() const { return readBuffer; }
    Buffer&& takeBuffer() { return std::move(readBuffer); }

    Signal<std::function<void(const SocketClient::SharedPtr&, Buffer&&)> >& readyRead() { return signalReadyRead; }
    Signal<std::function<void(const SocketClient::SharedPtr&, const String&, uint16_t, Buffer&&)> >& readyReadFrom() { return signalReadyReadFrom; }
    Signal<std::function<void(const SocketClient::SharedPtr&)> >& connected() { return signalConnected; }
    Signal<std::function<void(const SocketClient::SharedPtr&)> >& disconnected() { return signalDisconnected; }
    Signal<std::function<void(const SocketClient::SharedPtr&, int)> >& bytesWritten() { return signalBytesWritten; }

    enum Error {
        InitializeError,
        DnsError,
        ConnectError,
        BindError,
        ReadError,
        WriteError,
        EventLoopError
    };
    Signal<std::function<void(const SocketClient::SharedPtr&, Error)> >& error() { return signalError; }

    enum FlagMode { FlagAppend, FlagOverwrite };
    static bool setFlags(int fd, int flag, int getcmd, int setcmd, FlagMode mode = FlagAppend);

    double mbpsWritten() const;
    bool logsEnabled() const { return mLogsEnabled; }
    void setLogsEnabled(bool on) { mLogsEnabled = on; }
private:
    bool init(unsigned int mode);

    int fd;
    uint16_t socketPort;
    State socketState;
    unsigned int socketMode;
    WriteMode wMode;
    bool writeWait;
    String address;
    bool blocking;
    bool mLogsEnabled;

    Signal<std::function<void(const SocketClient::SharedPtr&, Buffer&&)> > signalReadyRead;
    Signal<std::function<void(const SocketClient::SharedPtr&, const String&, uint16_t, Buffer&&)> > signalReadyReadFrom;
    Signal<std::function<void(const SocketClient::SharedPtr&)> >signalConnected, signalDisconnected;
    Signal<std::function<void(const SocketClient::SharedPtr&, Error)> > signalError;
    Signal<std::function<void(const SocketClient::SharedPtr&, int)> > signalBytesWritten;
    void bytesWritten(const SocketClient::SharedPtr &socket, uint64_t bytes);
    Buffer readBuffer, writeBuffer;
    size_t writeOffset;

    int writeData(const unsigned char *data, int size);
    void socketCallback(int, int);

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

    friend class Resolver;
};

#endif
