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

    enum Mode { Tcp, Unix };

    SocketClient(Mode mode);
    SocketClient(int fd);
    ~SocketClient();

    enum State { Disconnected, Connecting, Connected };
    State state() const { return socketState; }

    bool connect(const std::string& path); // UNIX
    bool connect(const std::string& host, uint16_t port); // TCP
    bool isConnected() const { return fd != -1; }

    void close();

    bool write(const unsigned char* data, unsigned int num);
    bool write(const std::string& data) { return write(reinterpret_cast<const unsigned char*>(&data[0]), data.size()); }

    const Buffer& buffer() const { return readBuffer; }
    Buffer&& buffer() { return std::move(readBuffer); }

    Signal<std::function<void(SocketClient::SharedPtr&)> >& readyRead() { return signalReadyRead; }
    Signal<std::function<void(const SocketClient::SharedPtr&)> >& connected() { return signalConnected; }
    Signal<std::function<void(const SocketClient::SharedPtr&)> >& disconnected() { return signalDisconnected; }
    Signal<std::function<void(const SocketClient::SharedPtr&, int)> >& bytesWritten() { return signalBytesWritten; }

    enum Error { InitializeError, DnsError, ConnectError, ReadError, WriteError };
    Signal<std::function<void(const SocketClient::SharedPtr&, Error)> >& error() { return signalError; }

private:
    int fd;
    State socketState;
    bool writeWait;
    Signal<std::function<void(SocketClient::SharedPtr&)> > signalReadyRead;
    Signal<std::function<void(const SocketClient::SharedPtr&)> >signalConnected, signalDisconnected;
    Signal<std::function<void(const SocketClient::SharedPtr&, Error)> > signalError;
    Signal<std::function<void(const SocketClient::SharedPtr&, int)> > signalBytesWritten;
    Buffer readBuffer, writeBuffer;

    void socketCallback(int, int);
};

#endif
