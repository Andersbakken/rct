#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "SignalSlot.h"
#include "SocketClient.h"
#include <memory>
#include <string>
#include <queue>

struct sockaddr;

class SocketServer
{
public:
    typedef std::shared_ptr<SocketServer> SharedPtr;
    typedef std::weak_ptr<SocketServer> WeakPtr;

    SocketServer();
    ~SocketServer();

    void close();
    bool listen(uint16_t port); // TCP
    bool listen(const std::string& path); // UNIX

    SocketClient::SharedPtr nextConnection();

    Signal<std::function<void(SocketServer*)> >& newConnection() { return serverNewConnection; }

    enum Error { InitializeError, BindError, ListenError, AcceptError };
    Signal<std::function<void(SocketServer*, Error)> >& error() { return serverError; }

private:
    void socketCallback(int fd, int mode);
    bool commonListen(sockaddr* addr, size_t size);

private:
    int fd;
    std::queue<int> accepted;
    Signal<std::function<void(SocketServer*)> > serverNewConnection;
    Signal<std::function<void(SocketServer*, Error)> > serverError;
};

#endif
