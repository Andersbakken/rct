#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include "SignalSlot.h"
#include "Buffer.h"
#include <string>
#include <memory>

class SocketClient : public std::enable_shared_from_this<SocketClient>
{
public:
    typedef std::shared_ptr<SocketClient> SharedPtr;
    typedef std::weak_ptr<SocketClient> WeakPtr;

    enum Mode {
        Tcp = 0x1,
        Udp = 0x2,
        Unix = 0x4,
        IPv6 = 0x8
    };

    SocketClient(Mode mode);
    SocketClient(int fd, Mode mode);
    ~SocketClient();

    enum State { Disconnected, Connecting, Connected };
    State state() const { return socketState; }

    bool connect(const std::string& path); // UNIX
    bool connect(const std::string& host, uint16_t port); // TCP
    bool bind(uint16_t port); // UDP

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
    bool write(const unsigned char* data, unsigned int num);
    bool write(const std::string& data) { return write(reinterpret_cast<const unsigned char*>(&data[0]), data.size()); }
    bool peer(std::string* ip, uint16_t* port = 0);

    // UDP
    bool writeTo(const std::string& host, uint16_t port, const unsigned char* data, unsigned int num);
    bool writeTo(const std::string& host, uint16_t port, const std::string& data)
    {
        return writeTo(host, port, reinterpret_cast<const unsigned char*>(&data[0]), data.size());
    }

    // UDP Multicast
    bool addMembership(const std::string& ip);
    bool dropMembership(const std::string& ip);
    void setMulticastLoop(bool loop);

    const Buffer& buffer() const { return readBuffer; }
    Buffer&& takeBuffer() { return std::move(readBuffer); }

    Signal<std::function<void(SocketClient::SharedPtr&, Buffer&&)> >& readyRead() { return signalReadyRead; }
    Signal<std::function<void(SocketClient::SharedPtr&, const std::string&, uint16_t, Buffer&&)> >& readyReadFrom() { return signalReadyReadFrom; }
    Signal<std::function<void(const SocketClient::SharedPtr&)> >& connected() { return signalConnected; }
    Signal<std::function<void(const SocketClient::SharedPtr&)> >& disconnected() { return signalDisconnected; }
    Signal<std::function<void(const SocketClient::SharedPtr&, int)> >& bytesWritten() { return signalBytesWritten; }

    enum Error { InitializeError, DnsError, ConnectError, BindError, ReadError, WriteError, EventLoopError };
    Signal<std::function<void(const SocketClient::SharedPtr&, Error)> >& error() { return signalError; }

private:
    int fd;
    State socketState;
    Mode socketMode;
    WriteMode wMode;
    bool writeWait;

    Signal<std::function<void(SocketClient::SharedPtr&, Buffer&&)> > signalReadyRead;
    Signal<std::function<void(SocketClient::SharedPtr&, const std::string&, uint16_t, Buffer&&)> > signalReadyReadFrom;
    Signal<std::function<void(const SocketClient::SharedPtr&)> >signalConnected, signalDisconnected;
    Signal<std::function<void(const SocketClient::SharedPtr&, Error)> > signalError;
    Signal<std::function<void(const SocketClient::SharedPtr&, int)> > signalBytesWritten;
    Buffer readBuffer, writeBuffer;

    int writeData(const unsigned char *data, int size);
    void socketCallback(int, int);

    friend class Resolver;
};

#endif
