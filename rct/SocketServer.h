#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "SignalSlot.h"
#include "SocketClient.h"
#include <memory>
#include <rct/Path.h>
#include <queue>

struct sockaddr;

class SocketServer
{
public:
    typedef std::shared_ptr<SocketServer> SharedPtr;
    typedef std::weak_ptr<SocketServer> WeakPtr;

    SocketServer();
    ~SocketServer();

    enum Mode { IPv4, IPv6 };

    void close();
    bool listen(uint16_t port, Mode mode = IPv4); // TCP
    bool listen(const Path &path); // UNIX
    bool listenfd(int fd);         // UNIX
    bool isListening() const { return fd != -1; }

    SocketClient::SharedPtr nextConnection();

    Signal<std::function<void(SocketServer*)> >& newConnection() { return serverNewConnection; }

    enum Error { InitializeError, BindError, ListenError, AcceptError };
    Signal<std::function<void(SocketServer*, Error)> >& error() { return serverError; }

private:
    void socketCallback(int fd, int mode);
    bool commonBindAndListen(sockaddr* addr, size_t size);
    bool commonListen();

private:
    int fd;
    bool isIPv6;
    Path path;
    std::queue<int> accepted;
    Signal<std::function<void(SocketServer*)> > serverNewConnection;
    Signal<std::function<void(SocketServer*, Error)> > serverError;
};

#endif
