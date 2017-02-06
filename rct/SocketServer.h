#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include <memory>
#include <queue>
#include <stdint.h>

#include <rct/Path.h>
#include <rct/SignalSlot.h>
#include <rct/SocketClient.h>

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
#ifndef _WIN32
    bool listen(const Path &path); // UNIX
    bool listenFD(int fd);         // UNIX
#endif
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
